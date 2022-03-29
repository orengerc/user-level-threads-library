#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include "uthreads.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <algorithm>

/**********************************************************************************************************************
 *****************************************   Constants & Defines  *****************************************************
 *********************************************************************************************************************/

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif

#define SUCCESS EXIT_SUCCESS
#define FAILURE EXIT_FAILURE

/**********************************************************************************************************************
 ******************************************   Data Structures  ********************************************************
 *********************************************************************************************************************/

enum status
{
    MAIN,
    READY,
    RUNNING,
    BLOCKED
};

class UThread
{
public:
    int tid_;
    status status_;
    sigjmp_buf env_;
    char stack_[STACK_SIZE] = {0};

    UThread() : tid_(0), env_{0}, status_(MAIN)
    {
        sigsetjmp(env_, 1);
        sigemptyset(&env_->__saved_mask);
    }

    UThread(int tid, thread_entry_point entry_point) : tid_(tid), env_{0}, status_(READY)
    {
        //save thread configuration
        address_t sp = (address_t) stack_ + STACK_SIZE - sizeof(address_t);  //change for heap?
        address_t pc = (address_t) entry_point;
        sigsetjmp(env_, 1);
        (env_->__jmpbuf)[JB_SP] = translate_address(sp);
        (env_->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&env_->__saved_mask);  //needed?
    }
};

std::unordered_map<int, UThread *> all_threads_{};  //should allocate threads on the heap or stack???
std::unordered_set<int> blocked_tids_{};            //unordered because we want fast access
std::deque<int> ready_tids_{};
std::set<int> free_ids_{};                          //set saves the items in increasing order
int running_tid_{};
int quantum_{};
struct sigaction sa{};
struct itimerval timer{};

/**********************************************************************************************************************
 **********************************************   Functions    ********************************************************
 *********************************************************************************************************************/

int initialize_main_thread()
{
    //allocate new UThread for main thread
    auto new_uthread = new(std::nothrow) UThread;  // create new thread
    if (!new_uthread)
    {
        return FAILURE;
    }

    //add it to the map
    all_threads_[0] = new_uthread;

    return SUCCESS;
}

void cleanup()
{
    //deallocates all threads that were on the heap
    for (auto &pair: all_threads_)
    {
        delete pair.second;
    }
}

int start_timer()
{
    // Configure the timer to expire every quantum micro-seconds
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = quantum_;

    // Start a virtual timer
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        return FAILURE;
    }

    return SUCCESS;
}

int reset_timer()
{
    timer = {0};
    start_timer();
}

int switch_ready_to_running()
{
    //save environment of current running thread, only if save succeeded, proceed to jump
    if (sigsetjmp(all_threads_[running_tid_]->env_, 1))
    {
        /** deal with the error */
        cleanup();
        exit(EXIT_FAILURE);
    }

    //move first in ready to running
    running_tid_ = ready_tids_.front();

    //erase first in ready
    ready_tids_.pop_front();

    //reset timer
    reset_timer();

    //jump to new running tid
    siglongjmp(all_threads_[running_tid_]->env_, 1);  //need to check return value??
}

int time_switch()
{
    //add running thread to ready queue
    ready_tids_.emplace_back(running_tid_);

    //switch first in ready to running
    return switch_ready_to_running();
}

int initialize_sigaction()
{
    sa.sa_handler = reinterpret_cast<_sig_func_ptr>(&time_switch);  //good cast??
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        /** deal with the error */
        return FAILURE;
    }

    //add more actions

    return SUCCESS;
}

int uthread_init(int quantum_usecs)
{
    //init main thread - tid=0, directly into running
    if (SUCCESS != initialize_main_thread())
    {
        cleanup();
        exit(EXIT_FAILURE);
    }

    //init free_ids set
    for (int i = 1; i <= MAX_THREAD_NUM; ++i) free_ids_.insert(i);

    /** check input */
    if (quantum_usecs <= 0)
    {
        /** deal with quantum_usecs error */
        return FAILURE;
    }

    quantum_ = quantum_usecs;

    /** initialize sig-actions */
    if (SUCCESS != initialize_sigaction())
    {
        /** deal with setitimer error */
        return FAILURE;
    }

    /** initialize timer */
    if (SUCCESS != start_timer())
    {
        /** deal with setitimer error */
        return FAILURE;
    }

    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point)
{
    if (free_ids_.empty())
    {
        /** deal with the error */
        return FAILURE;
    }

    auto new_uthread = new(std::nothrow) UThread{*free_ids_.begin(), entry_point};  // allocate new thread
    if (!new_uthread)
    {
        cleanup();
        exit(EXIT_FAILURE);
    }
    free_ids_.erase(free_ids_.begin());  // erase the tid

    //add new thread to all_threads_ and ready queue
    all_threads_.emplace(new_uthread->tid_, new_uthread);
    ready_tids_.emplace_back(new_uthread->tid_);

    return SUCCESS;
}

int uthread_terminate(int tid)
{
    //main thread termination is illegal
    if (0 == tid)
    {
        cleanup();
        exit(SUCCESS);
    }

    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
    {
        /** deal with the error */
        return FAILURE;
    }

    //otherwise:
    switch (all_threads_[tid]->status_)
    {
        case MAIN:
            //irrelevant
            break;
        case RUNNING:
            //what to do??
            (void) (0);
            break;
        case READY:
            ready_tids_.erase(std::find(ready_tids_.begin(), ready_tids_.end(), tid));
            break;
        case BLOCKED:
            blocked_tids_.erase(tid);
            break;
    }

    //return tid to free_tids
    free_ids_.insert(tid);

    //release memory of the terminated thread
    delete all_threads_[tid];

    //switch tid to nex_tid

    //start timer again
    reset_timer();

    return SUCCESS;
}

int uthread_block(int tid)
{
    //try to block the main thread
    if (0 == tid)
    {
        /** deal with the error */
        return FAILURE;
    }

    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
    {
        /** deal with the error */
        return FAILURE;
    }

    //otherwise:
    switch (all_threads_[tid]->status_)
    {
        case RUNNING:
            blocked_tids_.insert(tid);
            if (SUCCESS != switch_ready_to_running())
            {
                /** deal with the error */
                return FAILURE;
            }
            break;
        case READY:
            ready_tids_.erase(std::find(ready_tids_.begin(), ready_tids_.end(), tid));
            break;
        case BLOCKED:
            blocked_tids_.erase(tid);
            break;
        case MAIN:
            //irrelevant
            break;
    }

    return SUCCESS;
}

int uthread_resume(int tid)
{
    return SUCCESS;
}

int uthread_sleep(int num_quantums)
{
    return SUCCESS;
}

int uthread_get_tid()
{
    return SUCCESS;
}

int uthread_get_total_quantums()
{
    return SUCCESS;
}

int uthread_get_quantums(int tid)
{
    return SUCCESS;
}

#pragma clang diagnostic pop