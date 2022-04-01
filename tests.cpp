#include <gtest/gtest.h>
#include "uthreads.h"

#define ERROR -1
#define SUCCESS 0
#define MAIN_THREAD 0

void Thread()
{
    std::cout << "in default thread\n";
    while (true)
    {}
}

TEST(ReturnValues, ReturnValues)
{
    /** init */
    const int num_quants = 333;
    ASSERT_EQ(uthread_init(-7), ERROR);  //negative quants
    ASSERT_EQ(uthread_init(num_quants), SUCCESS);

    /** spawn */
    //create MAX threads, for each one check returns proper id
    for (int i = 1; i <= MAX_THREAD_NUM; ++i)
    {
        ASSERT_EQ(uthread_spawn(Thread), i);  //return value is tid
    }
    //check that it refuses to create the 101'th thread
    ASSERT_EQ(uthread_spawn(Thread), ERROR);  //more the MAX

    /** terminate */
    const int terminated = 17;
    const int irrelevant_tid = MAX_THREAD_NUM + 34;
    ASSERT_EQ(uthread_terminate(irrelevant_tid), ERROR);
    ASSERT_EQ(uthread_terminate(terminated), SUCCESS);

    /** block */
    ASSERT_EQ(uthread_block(MAIN_THREAD), ERROR);  //main thread
    ASSERT_EQ(uthread_block(terminated), ERROR);  //thread doesn't exist
    ASSERT_EQ(uthread_block(irrelevant_tid), ERROR);  //thread doesn't exist
    const int blocked = uthread_get_tid();
    ASSERT_EQ(uthread_block(blocked), SUCCESS); //block myself is ok
    ASSERT_EQ(uthread_block(blocked), SUCCESS); //block someone twice has no effect

    /** resume */
    ASSERT_EQ(uthread_resume(terminated), ERROR); //was terminated
    ASSERT_EQ(uthread_resume(irrelevant_tid), ERROR);  //doesn't exist
    ASSERT_EQ(uthread_resume(1), SUCCESS);  //running/ready
    ASSERT_EQ(uthread_resume(MAIN_THREAD), SUCCESS);  //main
    ASSERT_EQ(uthread_resume(blocked), SUCCESS);  //blocked
}

TEST(Terminate, ActuallyTerminated)
{
    //check 17 was deleted and spawn gave us 17 which is free
    ASSERT_EQ(uthread_spawn(Thread), 17);
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
