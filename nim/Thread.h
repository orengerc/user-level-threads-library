//
// Created by nimrod on 24/04/2021.
//

#ifndef THREADS_PROJECT_THREAD_H
#define THREADS_PROJECT_THREAD_H

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/time.h>
#include "uthreads.h"
#include <iostream>

typedef unsigned long address_t;

// threads states
enum states {READY, RUNNING, BLOCKED,MUTEX_BLOCKED, NONE};

/**
 * class for holding threads attributes
 */
class Thread
{
private:
    int _id;
    int _quantums;
    sigjmp_buf _env;
    char _stack[STACK_SIZE];


public:
    /**
     * default constructor for main thread
     */
    Thread();

    /**
     * constructor for a thread object
     * @param id the id given to the thread
     * @param f the function the thread runs
     */
    Thread(int id, void (*f)(void));

    /**
     * in charge of saving the current environment of the thread
     * @return If successful direct invocation returns 0. If  sigsetjmp() is  returning from a call to siglongjmp(),
 * returns a non-zero value.
     */
    int saveEnv();

    /**
     * changes the thread being run to this thread
     */
    void startThread();

    /**
     * gets the id of the thread
     * @return the if of the thread
     */
    int getId() const;

    /**
     * gets the number of quantums that the thread has run for
     * @return the number of quantums
     */
    int getQuantum() const;

    /**
     * increases the quantums of the thread by one
     */
    void increaseQuantum();

};


#endif //THREADS_PROJECT_THREAD_H
