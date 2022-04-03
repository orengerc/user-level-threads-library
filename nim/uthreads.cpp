#include "uthreads.h"
#include <iostream>
#include <deque>
#include <unordered_map>
#include "Thread.h"
#include <string>
#include <algorithm>
#include <sys/time.h>

#define FAIL -1
#define SUCCESS 0
#define MAIN_THREAD 0
#define SEC_TO_MICRO 1000000
#define PREFIX_SYSTEM_ERROR "system error: "
#define PREFIX_LIBRARY_ERROR "thread library error: "
#define INIT_ERROR "quantum_usecs must be positive integer"
#define SPAWN_ERROR "max threads running has been reached"
#define TERMINATE_ERROR "the given id to terminate was not found"
#define BLOCK_ERROR "the given id to block was not found"
#define BLOCK_ZERO_ERROR "not allowed to block main thread"
#define MEMORY_ERROR "memory allocation "
#define THREAD_LIBRARY_ERROR "thread library error "
#define TIMER_LIBRARY_ERROR "timer library error "
#define UNBLOCK_ERROR "unable to resume an unblocked thread, or a non existing thread"
#define MUTEX_BLOCK_SAME "mutex is already blocked with the same thread"
#define MUTEX_ALREADY_UNBLOCKED "mutex is already unblocked"
#define MUTEX_BLOCKED_BY_SOMEONE_ELSE "mutex is already blocked by a different thread"
#define GET_QUANTUM_ERR "get quantums got wrong id"
void printErrors(std::string msg);
void systemError(std::string msg);
int getId();
void alarmSigHandler(int sig_num);
Thread* freeThread(int id);
void freeAllThread();
void switchThread(states moveTo, bool initAlarm);
void run_timer();

struct mutexBlock
{
    bool blocked;
    Thread* thread_block;
};

Thread* running;
std::deque<Thread*> ready;
std::unordered_map<int,Thread*>  mutexBlocked;
std::unordered_map<int,Thread*> blocked;
std::unordered_map<int,Thread*> allThreads;
struct itimerval timer;
struct sigaction sa;
sigset_t set;
int quantum_time;
int total_quantum = 0;
struct mutexBlock mutex;

/**
 * blocks the alarm virtual signal
 */
void block()
{
    sigset_t oset;
    // first put set as blocking, afterwards put oset back as a blocked signal set as we don't know
    // what was there beforehand and how important it was
    if (sigprocmask(SIG_SETMASK, &set, &oset) == FAIL || sigprocmask(SIG_BLOCK, &oset, NULL) == FAIL)
    {
        systemError(THREAD_LIBRARY_ERROR);
    }

}

/**
 * unblocks the alarm virtual signal
 */
void unblock()
{
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAIL)
    {
        systemError(THREAD_LIBRARY_ERROR);
    }

}

/**
 * library error caller
 * @param msg the message to print
 */
void printErrors(std::string msg)
{
    std::cerr << PREFIX_LIBRARY_ERROR << msg <<std::endl;
}

/**
 * system error caller
 * @param msg the message to print
 */
void systemError(std::string msg)
{
    freeAllThread();
    std::cerr << PREFIX_SYSTEM_ERROR << msg << std::endl;
    exit(EXIT_FAILURE);
}

/**
 * finds a free id
 * @return a free id to give to thread
 */
int getId()
{
    for (int i = 1; i < MAX_THREAD_NUM; i++)
    {
        if (allThreads.find(i) == allThreads.end())
        {
            return i;
        }
    }

    return MAX_THREAD_NUM - 1;
}

/**
 * searches the given ID in the deque of threads in ready state
 * @param id the id to search
 * @return an iterator at the place of the thread, else returns an iterator to the end of the queue
 */
std::deque<Thread*>::iterator findIdeque(int id)
{
    for (auto it = ready.begin(); it != ready.end(); it++)
    {
        if ((*it)->getId() == id)
        {
            return it;
        }
    }

    return ready.end();
}

/**
 * deletes the given thread from all DB.
 * @param id the id of the thread to delete
 * @return a pointer to the thread to delete.
 */
Thread* freeThread(int id)
{
    Thread *temp = allThreads[id]; // a pointer to the thread to delete
    allThreads.erase(id);

    if (blocked.find(id) != blocked.end()) // del from blocked DB
    {
        blocked.erase(id);
    }
    auto readyPtr = findIdeque(id);
    if (readyPtr != ready.end()) // del from ready DB
    {
        ready.erase(readyPtr);
    }
    if (mutexBlocked.find(id) != mutexBlocked.end()) // del from mutex blocked DB
    {
        mutexBlocked.erase(id);
    }

    return temp;
}

/**
 * deletes all threads.
 */
void freeAllThread()
{
    for(int i = 0; i < MAX_THREAD_NUM; i++)
    {
        if (allThreads.find(i) != allThreads.end())
        {
            delete allThreads[i];
        }
    }
}

/**
 * switches the running thread
 * @param moveTo where to move the current running thread
 * @param initAlarm do we need to start the alarm all over again
 * @param jump should we jump to the new thread or not? the default is yes
 */
void switchThread(states moveTo, bool initAlarm, bool jump=true)
{
    Thread *goingOut = running; // the leaving thread

    if (moveTo == BLOCKED)
    {
        blocked[goingOut->getId()] = goingOut;
    }
    else if (moveTo == MUTEX_BLOCKED)
    {
        mutexBlocked[goingOut->getId()] = goingOut;
    }
    else if (moveTo == READY)
    {
        ready.push_back(goingOut);
    }

    goingOut->saveEnv();
    running = ready.front(); // the new thread to run
    ready.pop_front();

    if (initAlarm)
    {
        run_timer();
    }

    total_quantum++;

    if (jump)
    {
        running->startThread();
    }
}


/**
 * virtual alarm signal handler
 * @param sig_num the signal number received
 */
void alarmSigHandler(int sig_num)
{
    block();
    switchThread(READY, false, true);
    unblock();

}

/**
 * installs the handlers for the uthread library
 */
void install_handlers()
{
    sa.sa_handler = &alarmSigHandler;

    if (sigemptyset(&set) == FAIL || sigaddset(&set, SIGVTALRM) == FAIL ||
        sigaction(SIGVTALRM, &sa, NULL) == FAIL)
    {
        systemError(THREAD_LIBRARY_ERROR);
    }

}
/**
 * unblocks one of the threads that are wating fot the mutex. first, trys to find one which is not blocked,
 * if not found - unblocks from the mutex line a random one, which will get into the mutex once it will running.
 */
void mutex_unblock_random_thread()
{
    Thread *toSet = nullptr;

    if (!mutexBlocked.empty())
    {
        for (auto &iter: mutexBlocked)
        {
            if (blocked.find(iter.first) == blocked.end())
            {
                toSet = iter.second;
                break;
            }
        }

        if (toSet == nullptr) // if all threads are blocked
        {
            toSet = mutexBlocked.begin()->second; // take the first one.
        }

        mutex.blocked = true;
        mutex.thread_block = toSet;
        mutexBlocked.erase(toSet->getId());
        // only if he isn't blocked by another thread we must put him in ready
        if (blocked.find(toSet->getId()) == blocked.end())
        {
            ready.push_back(toSet);
        }
    }
    else // no one is waiting for the mutex
    {
        // no one to give the mutex to
        mutex.blocked = false;
        mutex.thread_block = nullptr;
    }
}

/**
 * runs the timer all over again
 */
void run_timer()
{
    timer.it_value.tv_sec = (int) ((quantum_time) / SEC_TO_MICRO);
    timer.it_value.tv_usec = (int) (quantum_time % SEC_TO_MICRO);
    timer.it_interval.tv_sec = (int) ((quantum_time) / SEC_TO_MICRO);
    timer.it_interval.tv_usec = (int) (quantum_time % SEC_TO_MICRO);

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) == FAIL)
    {
        systemError(TIMER_LIBRARY_ERROR);
    }
}
/**
 * initiolise the uthread library.
 * @param quantum_usecs - number of seconds for each quantum
 * @return 0 for success, -1 if failed.
 */
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        printErrors(INIT_ERROR);
        return FAIL;
    }

    mutex.blocked = false;
    mutex.thread_block = nullptr;

    Thread *mainT = nullptr;
    try
    {
        mainT = new Thread();
    }
    catch(std::bad_alloc & exception)
    {
        systemError(MEMORY_ERROR);
    }

    allThreads[0] = mainT;
    running = mainT;
    total_quantum++;

    quantum_time = quantum_usecs;
    install_handlers();
    run_timer();
    return SUCCESS;
}
/**
 * starts a new thread, and push it to the end of ready queue.
 * @param f - the funcion that the thread should start runing on.
 * @return thread id for success, -1 if failed.
 */
int uthread_spawn(void (*f)(void))
{
    block();

    if (allThreads.size() + 1 > MAX_THREAD_NUM) // reached max number of threads
    {
        printErrors(SPAWN_ERROR);
        return FAIL;
    }

    int id = getId(); // sets a new if for the thread.

    Thread *thread = nullptr;
    try
    {
        thread = new Thread(id, f);
    }
    catch(std::bad_alloc & exception)
    {
        systemError(MEMORY_ERROR);
    }

    allThreads[id] = thread;
    ready.push_back(thread);

    unblock();
    return id;
}

/**
 * deletes a thread with the given id.
 * @param tid - the id of the thread.
 * @return 0 for success, -1 if failed.
 */
int uthread_terminate(int tid)
{
    block();

    if (allThreads.find(tid) == allThreads.end()) // thread with that id wasn't found
    {
        printErrors(TERMINATE_ERROR);
        unblock();
        return FAIL;
    }

    if (tid == MAIN_THREAD) // it's the main thread. terminates all threads.
    {
        freeAllThread();
        exit(EXIT_SUCCESS);
    }

    // if we are terminating a thread with that runs in the mutex, than first change the thread holding mutex
    if (mutex.blocked && tid == mutex.thread_block->getId())
    {
        mutex_unblock_random_thread();
    }

    Thread* toDel = freeThread(tid); // deletes the thread from all DB, and return a pointer to the thread.
    if (tid == running->getId()) // if the thread is the running thread then switch the running thread.
    {
        switchThread(NONE, true, false);
        delete toDel;
        running->startThread();
    }
    else
    {
        delete toDel;
    }

    unblock();

    return SUCCESS;
}
/**
 * blocks the thread with the given id.
 * @param tid the id of the thread to delete
 * @return 0 for success, -1 if failed.
 */
int uthread_block(int tid)
{
    block();

    if (allThreads.find(tid) == allThreads.end()) // thread with that id wasn't found
    {
        printErrors(BLOCK_ERROR);
        unblock();
        return FAIL;
    }

    if (tid == MAIN_THREAD) // its the main thread
    {
        printErrors(BLOCK_ZERO_ERROR);
        unblock();
        return FAIL;
    }

    // check if the id is in ready deque
    auto it = findIdeque(tid);
    if (it != ready.end())
    {
        ready.erase(it);
        blocked[tid] = allThreads[tid];
    }
    // check if the thread is the one running now
    else if (running == allThreads[tid])
    {
        switchThread(BLOCKED, true, true);
    }
    else
    {
        // we will reach this else only if the thread is now in mutexblock
        blocked[tid] = allThreads[tid];
    }



    unblock();

    return SUCCESS;
}
/**
 * resums a blocked thread
 * @param tid the id of the thread to resume
 * @return 0 for success, -1 if failed.
 */
int uthread_resume(int tid)
{
    block();
    auto blockedItr = blocked.find(tid);
    if (allThreads.find(tid) == allThreads.end()) //  thread with that id wasn't found
    {
        printErrors(UNBLOCK_ERROR);
        unblock();
        return FAIL;
    }
    // check that the thread is indeed blocked
    if (blocked.find(tid) != blocked.end())
    {
        Thread * current = blocked[tid];
        // check if the the thread was in mutexblock. If yes we can't put it in ready
        if (mutexBlocked.find(tid) == mutexBlocked.end())
        {
            ready.push_back(current);
        }
        blocked.erase(blockedItr);
    }

    unblock();
    return SUCCESS;
}
/**
 * returns the id of the running  thread
 */
int uthread_get_tid()
{
    block();
    int temp = running->getId();
    unblock();
    return temp;
}
/**
 * returns the total number od quantums that the thread with that id ran.
 * @param tid the id of the thread to cheack
 * @return total number of quantums
 */
int uthread_get_quantums(int tid)
{
    block();
    if (allThreads.find(tid) == allThreads.end()) //  thread with that id wasn't found
    {
        printErrors(GET_QUANTUM_ERR);
        unblock();
        return FAIL;
    }
    int temp = allThreads[tid]->getQuantum();
    unblock();
    return temp;
}
/**
 * returns the total number od quantums that all the threads ran together
 */
int uthread_get_total_quantums()
{
    block();
    int temp = total_quantum;
    unblock();
    return temp;
}
/**
 * trys to get into the mutex and lock it. if mutex is already locked, then the thread is switched to mutex_blocked and
 * waits for its turn in the mutex.
 * @return  0 for success, -1 if failed.
 */
int uthread_mutex_lock()
{
    block();

    Thread * current = running;
    if (mutex.blocked)
    {
        if (mutex.thread_block->getId() == current->getId()) // if the thread that is currently running in the mutex
            // is trying to lock the mutex again
        {
            printErrors(MUTEX_BLOCK_SAME);
            unblock();
            return FAIL;
        }
        mutexBlocked[current->getId()] = current;
        switchThread(MUTEX_BLOCKED, true, true);
    }
    else // mutex is free
    {
        mutex.blocked = true;
        mutex.thread_block = current;
    }

    unblock();
    return SUCCESS;
}
/**
 * unlocks the mutex. if other threads are waiting for it, enters one of them (randomly) how os not blocked.
 * @return 0 for success, -1 if failed.
 */
int uthread_mutex_unlock()
{
    block();

    if (!mutex.blocked) // mutex is not blocked
    {
        printErrors(MUTEX_ALREADY_UNBLOCKED);
        unblock();
        return FAIL;
    }

    // only the locking thread can unlock the mutex
    if (running->getId() != mutex.thread_block->getId())
    {
        printErrors(MUTEX_BLOCKED_BY_SOMEONE_ELSE);
        unblock();
        return FAIL;
    }

    mutex_unblock_random_thread();

    unblock();
    return  SUCCESS;
}