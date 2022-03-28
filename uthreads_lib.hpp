#include <iostream>
#include <setjmp.h>
#include <cstdlib>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <set>

#ifndef OS_2022_EX2_DATA_STRUCTURES_H
#define OS_2022_EX2_DATA_STRUCTURES_H

#define DEFAULT_QUANTUM -1  //good default value?

/** Holds a single user-level-thread. */
class UThread {
private:
    size_t tid_;
    sigjmp_buf env_;  //what library to include for this guy?
public:

    UThread(size_t tid, void (*entry_point)());     //constructor, sets up the environment of the thread

    size_t get_tid() const { return tid_; };

    void set_tid(size_t new_tid) { tid_ = new_tid; }
};

/** Manages UThreads using proper data-structures. */
class UthreadsLib {
public:

    UthreadsLib() : UthreadsLib(DEFAULT_QUANTUM) {}

    explicit UthreadsLib(int quantum_secs);

    //getters and setters

    //add new thread (called from spawn)

    //block thread

    //more functions we need

    //class variables (should be private?)
    std::unordered_map<int, UThread *> all_threads_ = {};  //should allocate threads on the heap or stack???
    std::unordered_set<int> blocked_tids_ = {};            //unordered because we want fast access
    std::queue<int> ready_tids_ = {};
    std::set<int> free_ids_ = {};                          //set saves the items in increasing order
    int running_tid_ = {};
    int quantum_ = {};
};

#endif //OS_2022_EX2_DATA_STRUCTURES_H
