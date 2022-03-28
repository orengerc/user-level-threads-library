#include "uthreads_lib.hpp"

UThread::UThread(size_t tid, void (*entry_point)()) {
    /* Constructor:
     * gets id, entrypoint
     * sets up the environment with the addresses and everything
     * saves with sigsetjmp
     * */
}

UthreadsLib::UthreadsLib(int quantum_secs) : quantum_(quantum_secs) {
    //init free_ids set
}

