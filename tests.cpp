#include <gtest/gtest.h>
#include "uthreads.h"

#define ERROR -1
#define SUCCESS 0
#define MAIN_THREAD 0

const int num_quants = 2;

void Thread()
{
    std::cout << "in Thread\n";
    int i=0;
    while (true)
    {  }
}

void BlockMyself(){
    uthread_block(uthread_get_tid());
}

void EndlessLoop(){
    while(true){}
}


#define NUM_THREADS 4
#define RUN 0
#define DONE 1

char thread_status[NUM_THREADS];
void halt()
{
    while (true)
    {}
}
int next_thread()
{
    return (uthread_get_tid() + 1) % NUM_THREADS;
}
void ThreadJona2(){
    uthread_block(uthread_get_tid());

    for (int i = 0; i < 50; i++)
    {
        uthread_resume(next_thread());
    }

    thread_status[uthread_get_tid()] = DONE;

    halt();
}
bool all_done()
{
    bool res = true;
    for (int i = 1; i < NUM_THREADS; i++)
    {
        res = res && (thread_status[i] == DONE);
    }
    return res;
}

void EndlessLoopB(){
    while(true){std::cout<<"B ";}
}


/** BASIC INPUT CHECK TESTS */

TEST(InputCheck, uthread_init){
    EXPECT_EQ(uthread_init(-7), ERROR);
    ASSERT_EQ(uthread_init(num_quants), SUCCESS);
    EXPECT_EQ(uthread_get_quantums(0), 1);
}

TEST(InputCheck, uthread_spawn){
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);
    }

    //create MAX threads, for each one check returns proper id
    for (int i = 1; i < MAX_THREAD_NUM; ++i)
    {
        EXPECT_EQ(uthread_spawn(Thread), i);  //return value is tid
    }
    //check that it refuses to create the 101'th thread
    EXPECT_EQ(uthread_spawn(Thread), ERROR);  //more the MAX
}

TEST(InputCheck, uthread_terminate){
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

        //create a thread
        ASSERT_EQ(uthread_spawn(Thread), 1);
    }

    EXPECT_EQ(uthread_terminate(137), ERROR);
    EXPECT_EQ(uthread_terminate(1), SUCCESS);
}

TEST(InputCheck, uthread_block)
{
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

        //create 2 threads
        ASSERT_EQ(uthread_spawn(Thread), 1);
        ASSERT_EQ(uthread_spawn(Thread), 2);
    }

    //bad inputs:
    EXPECT_EQ(uthread_block(MAIN_THREAD), ERROR);  //main thread
    EXPECT_EQ(uthread_block(3), ERROR);  //thread doesn't exist
    EXPECT_EQ(uthread_block(137), ERROR);  //thread doesn't exist

    //good inputs:
    EXPECT_EQ(uthread_block(1), SUCCESS); //blocking some thread
    EXPECT_EQ(uthread_block(1), SUCCESS); //block someone twice
    if(uthread_get_tid()){
        EXPECT_EQ(uthread_block(uthread_get_tid()), SUCCESS); //blocking myself
    }
}

TEST(InputCheck, uthread_resume)
{
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

        //create 3 threads
        ASSERT_EQ(uthread_spawn(Thread), 1);
        ASSERT_EQ(uthread_spawn(Thread), 2);
        ASSERT_EQ(uthread_spawn(Thread), 3);

        //block 1, terminate 1
        ASSERT_EQ(uthread_terminate(1), SUCCESS);
        ASSERT_EQ(uthread_block(2), SUCCESS);
    }

    EXPECT_EQ(uthread_resume(1), ERROR); //was terminated
    EXPECT_EQ(uthread_resume(137), ERROR);  //doesn't exist
    EXPECT_EQ(uthread_resume(3), SUCCESS);  //running/ready
    EXPECT_EQ(uthread_resume(0), SUCCESS);  //main
    EXPECT_EQ(uthread_resume(2), SUCCESS);  //blocked
}


/** FUNCTIONALITY TESTS */

void  wait_10_quantums(){
    int curr = uthread_get_total_quantums();
    while (curr + 10 >= uthread_get_total_quantums())
    {
        std::cout<< uthread_get_tid() << "\n";
    }
    return;
}

TEST(Test1, TerminateAndSpawn){
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

        //create threads
        ASSERT_EQ(uthread_spawn(BlockMyself), 1);
        ASSERT_EQ(uthread_spawn(EndlessLoop), 2);
        ASSERT_EQ(uthread_spawn(EndlessLoop), 3);
        ASSERT_EQ(uthread_spawn(BlockMyself), 4);
        ASSERT_EQ(uthread_spawn(EndlessLoop), 5);
        ASSERT_EQ(uthread_spawn(BlockMyself), 6);
    }

    uthread_terminate(5);
    EXPECT_EQ(uthread_spawn(BlockMyself), 5);

    wait_10_quantums();


    uthread_terminate(5);
    EXPECT_EQ(uthread_spawn(BlockMyself), 5);

    uthread_terminate(2);
    EXPECT_EQ(uthread_spawn(EndlessLoop), 2);

    uthread_terminate(3);
    uthread_terminate(4);
    EXPECT_EQ(uthread_spawn(EndlessLoop), 3);
    EXPECT_EQ(uthread_spawn(BlockMyself), 4);
}

TEST(Test2, Jona2){
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

        //create threads
        ASSERT_EQ(uthread_spawn(ThreadJona2), 1);
        ASSERT_EQ(uthread_spawn(ThreadJona2), 2);
        ASSERT_EQ(uthread_spawn(ThreadJona2), 3);
    }

    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_status[i] = RUN;
    }

    while (!all_done())
    {
        uthread_resume(1);
    }
}


void T2(){
    halt();
}

void T3(){
    halt();
}


TEST(TestSleep, sleep){
    //setup:
    {
        //init library
        ASSERT_EQ(uthread_init(num_quants), SUCCESS);

    }

    static bool awake = false;

    auto T1 = [](){
        int quant = uthread_get_total_quantums();
        uthread_sleep(3);
        std::cout << uthread_get_total_quantums() - quant;
        awake = true;
    };
    auto T2 = [](){
        halt();
    };
    auto T3 = [](){
        halt();
    };

    //create 2 threads
    ASSERT_EQ(uthread_spawn(T1), 1);
    ASSERT_EQ(uthread_spawn(T2), 2);
    ASSERT_EQ(uthread_spawn(T3), 3);

    // next, we'll go to f, and then back here(since g was terminataed)
    wait_10_quantums();


}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
