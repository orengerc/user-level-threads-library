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
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <set>


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
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

//-----------------------------------------------------classes----------------------------------------------------------

#define DEFAULT_QUANTUM 0  //good default value??
#define MAX_THREADS 1500   //what's the actual max??
#define SUCCESS EXIT_SUCCESS
#define FAILURE EXIT_FAILURE

/** Class that implements a single user-level-thread. */
class UThread {
private:
    int tid_;
    sigjmp_buf env_;  //what library to include for this guy?
public:
    UThread(int tid, char *stack, thread_entry_point entry_point) : tid_(tid) {
        address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
        address_t pc = (address_t) entry_point;
        sigsetjmp(env_, 1);
        (env_->__jmpbuf)[JB_SP] = translate_address(sp);
        (env_->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&env_->__saved_mask);  //needed?
    }

    int get_tid() const { return tid_; };

    void set_tid(int new_tid) { tid_ = new_tid; }
};

/** Class that manages UThreads using proper data-structures. */
class UthreadsLib {
private:
    std::unordered_map<int, UThread> all_threads_ = {};  //should allocate threads on the heap or stack???
    std::unordered_set<int> blocked_tids_ = {};            //unordered because we want fast access
    std::queue<int> ready_tids_ = {};
    std::set<int> free_ids_ = {};                          //set saves the items in increasing order
    int running_tid_ = {};
    int quantum_ = DEFAULT_QUANTUM;
public:
    UthreadsLib() {
        //init free_ids set
        for (int i = 1; i <= MAX_THREADS; ++i) free_ids_.insert(i);
    }

    //getters and setters
    void set_quantum(int quantum) {
        quantum_ = quantum;
    }

    //add new thread (called from spawn)
    int create_new_thread(void *stack_p, thread_entry_point entry_point) {
        if (free_ids_.empty()) {
            return FAILURE;
        }

        UThread new_uthread = {*free_ids_.begin(), stack_p, entry_point};  // create new thread
        free_ids_.erase(free_ids_.begin());  // erase the tid

        //add new thread to all_threads_ and ready queue
        all_threads_.emplace(new_uthread.get_tid(),
                             new_uthread); /** will the thread object live outside of this scope?? */
        ready_tids_.push(new_uthread.get_tid());

        return SUCCESS;
    }

    //block thread

    //more functions we need
};

//----------------------------------------------------functions---------------------------------------------------------

UthreadsLib uthreads;

int uthread_init(int quantum_usecs) {
    uthreads.set_quantum(quantum_usecs);
    /** start timer */

    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point) {
    //allocate 4096 bytes of memory for stack of the new thread
    void *stack_p = malloc(STACK_SIZE);
    if (!stack_p) {
        /** deal with the error */
        return FAILURE;
    }

    int success = uthreads.create_new_thread((char*)stack_p, entry_point);
    if (SUCCESS != success) {
        /** deal with the error */
        return FAILURE;
    }

    return SUCCESS;
}

int uthread_terminate(int tid) {
    return SUCCESS;
}

int uthread_block(int tid) {
    return SUCCESS;
}

int uthread_resume(int tid) {
    return SUCCESS;
}

int uthread_sleep(int num_quantums) {
    return SUCCESS;
}

int uthread_get_tid() {
    return SUCCESS;
}

int uthread_get_total_quantums() {
    return SUCCESS;
}

int uthread_get_quantums(int tid) {
    return SUCCESS;
}

#pragma clang diagnostic pop