#include <iostream>
#include "uthreads.h"





void thread1()
{
    //uthread_block(3);
    printf("hello");
}


int main()
{
    int a = uthread_init(7000);
    uthread_spawn(thread1);
    while (true){
        if (a == 4){
            break;
        }
    }
    return 0;
}
