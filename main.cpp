#include <iostream>
#include "uthreads.h"





void thread1()
{
    uthread_block(3);
    printf("hello");
}


int main()
{
    uthread_init(7000);
    return 0;
}
