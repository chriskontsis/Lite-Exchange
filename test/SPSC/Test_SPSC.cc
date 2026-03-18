#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../../src/utility/SPSC_Queue.hpp"

TEST(SPSCQueueTest, TryConsumeEmptyReturnsFalse)
{
    SPSC_Queue<int> q(4);
    int out;
    EXPECT_FALSE(q.tryConsume(out));
}

TEST(SPSCQueueTest, PushAndPop)
{
    SPSC_Queue<int> q(4);
    q.push(42);
    EXPECT_EQ(q.pop(), 42);
}

TEST(SPSCQueueTest, FIFOOrder)
{   
    SPSC_Queue<int> q(8);
    for(int i = 0; i < 5; ++i) q.push(i);
    for(int i = 0; i < 5; ++i) EXPECT_EQ(q.pop(), i);
}

TEST(SPSCQueueTest, TryPublishWhenFull)
{
    SPSC_Queue<int> q(4);
    for(int i = 0; i < 4; ++i) EXPECT_TRUE(q.tryPublish(i));
    EXPECT_FALSE(q.tryPublish(99));
}

TEST(SPSCQueueTest, Wraparound)
{
    SPSC_Queue<int> q(4);
    for(int round = 0; round < 3; ++round) {
        for(int i = 0; i < 4; ++i) q.push(i);
        for(int i = 0; i < 4; ++i) EXPECT_EQ(q.pop(), i);
    }
}

TEST(SPSCQueueTest, ConcurrentProducerConsumer)
{
    SPSC_Queue<int> q(1024);
    const int N = 100000;
    std::vector<int> recv;
    recv.reserve(N);

    std::thread producer([&]() {
        for(int i = 0; i < N; ++i) q.push(i);
    });

    std::thread consumer([&]() {
        for(int i = 0; i < N; ++i)
            recv.push_back(q.pop());
    });

    producer.join();
    consumer.join();

    ASSERT_EQ((int)recv.size(), N);
    for(int i = 0; i < N; ++i) EXPECT_EQ(recv[i], i);
}