//
// Created by User on 24/04/2021.
//

#include "Thread.h"

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
/**
 * constructor for thread 0
 */
Thread::Thread(): _id(0), _quantums(1)
{
    sigsetjmp(_env, 1);
    sigemptyset(&_env->__saved_mask);
}
/**
 * constructor for all threads except for  thread 0
 * @param id the id of the new thread
 * @param f the function to start running on
 */
Thread::Thread(int id, void (*f)()): _id(id), _quantums(0)
{
    address_t sp, pc;

    sp = (address_t)_stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    sigsetjmp(_env, 1);
    (_env->__jmpbuf)[JB_SP] = translate_address(sp);
    (_env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&_env->__saved_mask);
}


int Thread::getId() const
{
    return _id;
}

int Thread::getQuantum() const
{
    return _quantums;
}
/**
 * increasing thread running time by 1 quantum
 */
void Thread::increaseQuantum()
{
    _quantums++;
}

/**
 * saves the environment of the thread before leaving running state
 * @return If successful direct invocation returns 0. If  sigsetjmp() is  returning from a call to siglongjmp(),
 * returns a non-zero value.
 */
int Thread::saveEnv()
{
    return sigsetjmp(_env, 1);
}
/**
 * starts a run of a thread
 */
void Thread::startThread()
{
    increaseQuantum();
    siglongjmp(_env, 1);
}
