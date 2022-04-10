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
#include <map>

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
#define FAILURE (-1)
#define MICRO_TO_SEC 1000000
#define SYSTEM_CALL_ERR "system error: "
#define MEMORY_ERR "Memory allocation failed\n"
#define TIMER_ERR "Setitimer failed\n"
#define LIBRARY_ERR "thread library error: "
#define INPUT_ERR "Invalid input\n"
#define MAX_THREADS_ERR "Passed the max num of threads\n"
#define TID_NOT_EXIST "This thread doesn't exist\n"
#define BLOCK_THREAT_ZERO "Can not block thread 0\n"
#define SLEEP_THREAD_ZERO "Can not sleep thread 0\n"
#define SIGS_ERROR "Signal library fails\n"




/**********************************************************************************************************************
 ******************************************   Data Structures  ********************************************************
 *********************************************************************************************************************/
enum status
{
    MAIN,
    READY,
    RUNNING,
    BLOCKED,
    ASLEEP,
    TERMINATED
};

class UThread
{
public:
    int tid_;
    status status_;
    sigjmp_buf env_;
    int num_of_quantums_running = {0};
    char stack_[STACK_SIZE] = {0};

    UThread() : tid_(0), env_{0}, status_(MAIN)
    {
      //Do we need to save the env for thread 0?
        num_of_quantums_running ++;
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

    void startRunning(){
        num_of_quantums_running++;
        siglongjmp(env_, 1);  //need to check return value??
    }

    int saveEnvironment()
    {
        int success = sigsetjmp(env_, 1);
        return success;
    }

    ~UThread(){
//        free(env_->__jmpbuf);
    }
};

std::unordered_map<int, UThread *> all_threads_{};  //should allocate threads on the heap or stack???
std::map<int, int> sleepers_{};
std::unordered_set<int> blocked_tids_{};            //unordered because we want fast access
std::deque<int> ready_tids_{};
std::set<int> free_ids_{};                          //set saves the items in increasing order
int total_quantums;
int running_tid_{};
int quantum_{};
struct sigaction sa{};
struct itimerval timer{};
sigset_t set;

/**********************************************************************************************************************
 **********************************************   Functions    ********************************************************
 *********************************************************************************************************************/

void initialize_main_thread()
{
    //allocate new UThread for main thread
    auto new_uthread = new(std::nothrow) UThread;  // create new thread
    if (!new_uthread)
    {
      std::cerr << SYSTEM_CALL_ERR << MEMORY_ERR;
      exit(EXIT_FAILURE);
    }

    //add it to the map
    all_threads_[0] = new_uthread;

}

void cleanup()
{
    //deallocates all threads that were on the heap
    for (auto &pair: all_threads_)
    {
        delete pair.second;
    }
}

void start_timer()
{
    int sec = quantum_/MICRO_TO_SEC;
    int micro = quantum_ % MICRO_TO_SEC;
    // Configure the timer to expire every quantum micro-seconds
    timer.it_value.tv_sec = sec;
    timer.it_value.tv_usec = micro;
    timer.it_interval.tv_sec = sec;
    timer.it_interval.tv_usec = micro;

    // Start a virtual timer
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << SYSTEM_CALL_ERR << TIMER_ERR;
        exit(EXIT_FAILURE);
    }

}

void reset_timer()
{
    timer = {0};
    start_timer();
}

void block_vtAlarm()
{
  if (sigemptyset(&set) == FAILURE){
      std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
      exit(EXIT_FAILURE);
  }
  if (sigaddset(&set, SIGVTALRM) == FAILURE){
      std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
      exit(EXIT_FAILURE);
  }
  if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE){
      std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
      exit(EXIT_FAILURE);
  }
}

void unblock_vtAlarm()
{
    if (sigemptyset(&set) == FAILURE){
        std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
        exit(EXIT_FAILURE);
    }
    if (sigaddset(&set, SIGVTALRM) == FAILURE){
        std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
        exit(EXIT_FAILURE);
    }
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE){
        std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
        exit(EXIT_FAILURE);
    }
}

void update_sleepers(){
    // does it work??
    for (auto it = sleepers_.begin(), next_it = it; it != sleepers_.end(); it = next_it){
        ++next_it;
        it->second --;
        if (it -> second == -1){
            if (blocked_tids_.find(it ->first) == blocked_tids_.end() ){
                all_threads_[it->first] ->status_ = READY;
                ready_tids_.emplace_back(it->first);
            } else {
                all_threads_[it->first] ->status_ = BLOCKED;
            }
            sleepers_.erase(it);
        }
    }
}

void insert_ready_to_running(){
    //move from ready to running.
    // what about empty ready qeueue??
    if (ready_tids_.empty()){
        running_tid_ = MAIN;
    }

    //move first in ready to running
    else{
        running_tid_ = ready_tids_.front();
        //erase first in ready
        ready_tids_.pop_front();
        all_threads_[running_tid_] -> status_ = RUNNING;
    }
}

void switch_threads(status to){

    //insert sleepers into ready queue
    update_sleepers();

    int orig_tid = running_tid_;

    switch (to) {
        case READY:
            ready_tids_.emplace_back(running_tid_);
            all_threads_[running_tid_] -> status_ = READY;
            break;
        case BLOCKED:
            blocked_tids_.insert(running_tid_);
            all_threads_[running_tid_] -> status_ = BLOCKED;
            break;
        case TERMINATED:
            insert_ready_to_running();
            total_quantums ++;
            goto jump;
    }

    //move first in ready to running
    insert_ready_to_running();
    total_quantums ++;

    //saveEnv
    if(!all_threads_[orig_tid]->saveEnvironment()){
        //jump to new thread
        jump:
        unblock_vtAlarm();
        all_threads_[running_tid_]->startRunning();
    }
}

void timer_handler(){
    block_vtAlarm();
    switch_threads(READY);
    unblock_vtAlarm();
}

void initialize_sigaction()
{
    sa.sa_handler = reinterpret_cast<__sighandler_t>(&timer_handler);  //good cast??
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << SYSTEM_CALL_ERR << SIGS_ERROR;
        exit(EXIT_FAILURE);
    }

}

int uthread_init(int quantum_usecs)
{
    //check input
    if (quantum_usecs <= 0)
    {
        std::cerr << LIBRARY_ERR << INPUT_ERR;
        return FAILURE;
    }

    //init main thread - tid=0, directly into running
    initialize_main_thread();

    //start count
    total_quantums = 1;

    //init free_ids set
    for (int i = 1; i < MAX_THREAD_NUM; ++i) free_ids_.insert(i);

    quantum_ = quantum_usecs;

    /** initialize sig-actions */
    initialize_sigaction();


    /** initialize timer */
    start_timer();

    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point)
{
    block_vtAlarm();
    if (free_ids_.empty())
    {
        std::cerr << LIBRARY_ERR << MAX_THREADS_ERR;
        return FAILURE;
    }

    auto new_uthread = new(std::nothrow) UThread{*free_ids_.begin(), entry_point};  // allocate new thread
    if (!new_uthread)
    {
        cleanup();
        std::cerr << SYSTEM_CALL_ERR << MEMORY_ERR;
        exit(EXIT_FAILURE);
    }
    free_ids_.erase(free_ids_.begin());  // erase the tid

    //add new thread to all_threads_ and ready queue
    all_threads_.emplace(new_uthread->tid_, new_uthread);
    ready_tids_.emplace_back(new_uthread->tid_);

    unblock_vtAlarm();
    return new_uthread->tid_;
}

int uthread_terminate(int tid)
{
    block_vtAlarm();

    //main thread termination is illegal
    if (0 == tid)
    {
        cleanup();
        exit(SUCCESS);
    }

    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
    {
        std::cerr << LIBRARY_ERR << TID_NOT_EXIST;
        return FAILURE;
    }

    //otherwise:

    //return tid to free_tids
    free_ids_.insert(tid);

    //save status
    auto status = all_threads_[tid]->status_;

    //erase terminated thread, and its release memory
    delete all_threads_[tid];
    all_threads_.erase(tid);

    switch (status)
    {
        case MAIN:
            //irrelevant
            break;
        case RUNNING:
            //start timer again
            reset_timer();
            //switch tid to nex_tid
            unblock_vtAlarm();
            switch_threads(TERMINATED);
            break;
        case READY:
            ready_tids_.erase(std::find(ready_tids_.begin(), ready_tids_.end(), tid));
            break;
        case BLOCKED:
            blocked_tids_.erase(tid);
            break;
    }

    unblock_vtAlarm();
    return SUCCESS;
}

int uthread_block(int tid)
{
    block_vtAlarm();

    //try to block the main thread
    if (0 == tid)
    {
        std::cerr << LIBRARY_ERR << BLOCK_THREAT_ZERO;
        return FAILURE;
    }

    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
    {
        std::cerr << LIBRARY_ERR << TID_NOT_EXIST;
        return FAILURE;
    }

    //otherwise:
    switch (all_threads_[tid]->status_)
    {
        case RUNNING:
            //start timer again
            reset_timer();
            switch_threads(BLOCKED);
            break;
        case READY:
            ready_tids_.erase(std::find(ready_tids_.begin(), ready_tids_.end(), tid));
            blocked_tids_.insert(tid);
            all_threads_[tid]->status_ = BLOCKED;
            break;
        case ASLEEP:
            blocked_tids_.insert(tid);
        case BLOCKED:
            // Do nothing
            break;
        case MAIN:
            //irrelevant
            break;
    }

    unblock_vtAlarm();
    return SUCCESS;
}

int uthread_resume(int tid)
{
    block_vtAlarm();

    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
    {
        std::cerr << LIBRARY_ERR << TID_NOT_EXIST;
        return FAILURE;
    }

    //otherwise:

    if (all_threads_[tid]->status_ == BLOCKED){

        blocked_tids_.erase(tid);
        ready_tids_.emplace_back(tid);
        all_threads_[tid]->status_ = READY;
    }

    if (all_threads_[tid]->status_ == ASLEEP){
        blocked_tids_.erase(tid);
    }

    unblock_vtAlarm();

    return SUCCESS;

}

int uthread_sleep(int num_quantums)
{
    block_vtAlarm();

    //try to block the main thread
    if (0 == running_tid_)
    {
        std::cerr << LIBRARY_ERR << SLEEP_THREAD_ZERO;
        return FAILURE;
    }

    if (0 > num_quantums)
    {
        std::cerr << LIBRARY_ERR << INPUT_ERR;
        return FAILURE;
    }
    all_threads_[running_tid_] ->status_ = ASLEEP;
    sleepers_.emplace(running_tid_, num_quantums);

    //start timer again
    reset_timer();
    switch_threads(ASLEEP);

    return SUCCESS;
}

int uthread_get_tid()
{
    // do we need to block?
    return running_tid_;

}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums(int tid)
{
    block_vtAlarm();
    //if thread doesn't exist, return failure
    if (all_threads_.find(tid) == all_threads_.end())
      {
        std::cerr << LIBRARY_ERR << TID_NOT_EXIST;
        return FAILURE;
      }
    unblock_vtAlarm();
    return all_threads_[tid] -> num_of_quantums_running;
}

#pragma clang diagnostic pop
