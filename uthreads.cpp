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


//-----------------------------------------------------classes----------------------------------------------------------

#define DEFAULT_QUANTUM -1  //good default value?

/** Class that implements a single user-level-thread. */
class UThread {
private:
    size_t tid_;
    sigjmp_buf env_;  //what library to include for this guy?
public:
    UThread(size_t tid, void (*entry_point)()) {
        /** Constructor:
         * gets id, entrypoint
         * sets up the environment with the addresses and everything
         * saves with sigsetjmp
         */
    }

    size_t get_tid() const { return tid_; };

    void set_tid(size_t new_tid) { tid_ = new_tid; }
};

/** Class that manages UThreads using proper data-structures. */
class UthreadsLib {
private:
    std::unordered_map<int, UThread *> all_threads_ = {};  //should allocate threads on the heap or stack???
    std::unordered_set<int> blocked_tids_ = {};            //unordered because we want fast access
    std::queue<int> ready_tids_ = {};
    std::set<int> free_ids_ = {};                          //set saves the items in increasing order
    int running_tid_ = {};
    int quantum_ = {};
public:
    UthreadsLib() : UthreadsLib(DEFAULT_QUANTUM) {}

    explicit UthreadsLib(int quantum_secs) : quantum_(quantum_secs) {
        //init free_ids set
    }

    //getters and setters

    //add new thread (called from spawn)

    //block thread

    //more functions we need
};

//----------------------------------------------------functions---------------------------------------------------------

int uthread_init(int quantum_usecs) {
    //initialize new UthreadLib with quantum_secs
}

int uthread_spawn(thread_entry_point entry_point) {
    //init thread with stack size 4096
    //add it to ready queue
}

int uthread_terminate(int tid) {

}

int uthread_block(int tid) {

}

int uthread_resume(int tid) {}

int uthread_sleep(int num_quantums) {}

int uthread_get_tid() {}

int uthread_get_total_quantums() {}

int uthread_get_quantums(int tid) {}

#pragma clang diagnostic pop