#include "core/RingBuffer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

using rfpulse::core::RingBuffer;

TEST(RingBuffer, RoundsCapacityUpToPowerOfTwo)
{
    RingBuffer<int> rb(10);
    EXPECT_EQ(rb.capacity(), 16u);
}

TEST(RingBuffer, EmptyPopFails)
{
    RingBuffer<int> rb(8);
    int value = 0;
    EXPECT_FALSE(rb.tryPop(value));
}

TEST(RingBuffer, PushThenPopPreservesValueAndOrder)
{
    RingBuffer<int> rb(8);
    ASSERT_TRUE(rb.tryPush(42));
    ASSERT_TRUE(rb.tryPush(43));

    int a = 0;
    int b = 0;
    ASSERT_TRUE(rb.tryPop(a));
    ASSERT_TRUE(rb.tryPop(b));
    EXPECT_EQ(a, 42);
    EXPECT_EQ(b, 43);

    int c = 0;
    EXPECT_FALSE(rb.tryPop(c));
}

TEST(RingBuffer, FullBufferRejectsPush)
{
    RingBuffer<int> rb(4); // capacidad real 4
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(rb.tryPush(i));
    }
    EXPECT_FALSE(rb.tryPush(99));
    EXPECT_EQ(rb.sizeApprox(), 4u);
}

TEST(RingBuffer, WrapAroundKeepsCorrectOrderAfterManyCycles)
{
    RingBuffer<int> rb(4);
    int out = 0;
    // Da varias vueltas completas al buffer para ejercitar el enmascarado de
    // indices tras desbordar el contador subyacente varias veces.
    for (int cycle = 0; cycle < 1000; ++cycle) {
        ASSERT_TRUE(rb.tryPush(cycle));
        ASSERT_TRUE(rb.tryPop(out));
        EXPECT_EQ(out, cycle);
    }
}

TEST(RingBuffer, BulkPushAndPopRespectAvailableSpace)
{
    RingBuffer<int> rb(8);
    const std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    const std::size_t pushed = rb.tryPushBulk(input.data(), input.size());
    EXPECT_EQ(pushed, 8u); // capacidad real 8, aunque se pidieran 10

    std::vector<int> output(8, 0);
    const std::size_t popped = rb.tryPopBulk(output.data(), output.size());
    EXPECT_EQ(popped, 8u);
    for (std::size_t i = 0; i < popped; ++i) {
        EXPECT_EQ(output[i], input[i]);
    }
}

TEST(RingBuffer, ConcurrentSingleProducerSingleConsumerDeliversAllValuesInOrder)
{
    constexpr int kTotal = 2'000'000;
    RingBuffer<int> rb(1024);

    std::vector<int> received;
    received.reserve(kTotal);

    std::thread producer([&rb]() {
        for (int i = 0; i < kTotal; ++i) {
            while (!rb.tryPush(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&rb, &received]() {
        int value = 0;
        int count = 0;
        while (count < kTotal) {
            if (rb.tryPop(value)) {
                received.push_back(value);
                ++count;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(kTotal));
    for (int i = 0; i < kTotal; ++i) {
        ASSERT_EQ(received[static_cast<std::size_t>(i)], i) << "desorden o perdida de datos en indice " << i;
    }
}
