#include "core/TripleBuffer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

using rfpulse::core::TripleBuffer;

TEST(TripleBuffer, ConsumeLatestFailsWhenNothingPublishedYet)
{
    TripleBuffer<int> buffer(0);
    EXPECT_FALSE(buffer.consumeLatest());
}

TEST(TripleBuffer, ConsumerSeesPublishedValue)
{
    TripleBuffer<int> buffer(0);
    buffer.writable() = 42;
    buffer.publish();

    ASSERT_TRUE(buffer.consumeLatest());
    EXPECT_EQ(buffer.latest(), 42);
}

TEST(TripleBuffer, ConsumerKeepsLastValueWhenNothingNewSincePreviousRead)
{
    TripleBuffer<int> buffer(0);
    buffer.writable() = 1;
    buffer.publish();
    ASSERT_TRUE(buffer.consumeLatest());
    EXPECT_EQ(buffer.latest(), 1);

    EXPECT_FALSE(buffer.consumeLatest()); // nada nuevo publicado
    EXPECT_EQ(buffer.latest(), 1); // sigue viendo el mismo valor
}

TEST(TripleBuffer, MultiplePublishesBeforeConsumeOnlyExposeTheLatest)
{
    TripleBuffer<int> buffer(0);
    for (int i = 1; i <= 5; ++i) {
        buffer.writable() = i;
        buffer.publish();
    }

    ASSERT_TRUE(buffer.consumeLatest());
    EXPECT_EQ(buffer.latest(), 5); // se pierden 1..4, es el comportamiento esperado
}

TEST(TripleBuffer, ConcurrentProducerConsumerNeverObservesTornOrStaleBeyondLast)
{
    constexpr int kIterations = 500'000;
    TripleBuffer<int> buffer(-1);

    std::atomic<bool> producerDone{false};
    std::thread producer([&]() {
        for (int i = 0; i < kIterations; ++i) {
            buffer.writable() = i;
            buffer.publish();
        }
        producerDone.store(true, std::memory_order_release);
    });

    int lastSeen = -1;
    int observations = 0;
    while (!producerDone.load(std::memory_order_acquire)) {
        if (buffer.consumeLatest()) {
            const int value = buffer.latest();
            // El consumidor puede saltarse valores intermedios (es el
            // comportamiento esperado de "ultimo valor gana"), pero nunca
            // debe ver un valor menor que el anterior (eso indicaria
            // corrupcion o una condicion de carrera real).
            ASSERT_GE(value, lastSeen) << "valor fuera de orden: retrocedio";
            lastSeen = value;
            ++observations;
        }
    }
    // drena cualquier ultimo valor publicado tras el join
    if (buffer.consumeLatest()) {
        const int value = buffer.latest();
        ASSERT_GE(value, lastSeen);
        lastSeen = value;
    }

    producer.join();
    EXPECT_GT(observations, 0);
    EXPECT_EQ(lastSeen, kIterations - 1) << "el consumidor nunca vio el ultimo valor publicado";
}
