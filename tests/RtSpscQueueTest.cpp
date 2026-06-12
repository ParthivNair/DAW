// Tests for the rt::SpscQueue wrapper (the canonical UI<->audio messaging
// primitive). Covers the cross-thread round-trip and the fails-when-full
// (never-grows) contract that keeps the RT side allocation-free.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "rt/SpscQueue.h"

TEST_CASE ("SpscQueue: single-thread FIFO round-trip", "[rt][spsc]")
{
    daw::rt::SpscQueue<int> queue { 16 };
    auto tx = queue.producer();
    auto rx = queue.consumer();

    for (int i = 0; i < 8; ++i)
        REQUIRE (tx.try_push (i));

    for (int i = 0; i < 8; ++i)
    {
        int out = -1;
        REQUIRE (rx.try_pop (out));
        REQUIRE (out == i);
    }

    int drained = 0;
    REQUIRE_FALSE (rx.try_pop (drained)); // empty now
}

TEST_CASE ("SpscQueue: try_push returns false when full instead of growing", "[rt][spsc]")
{
    // Pre-size small; push until full and confirm it refuses rather than grows.
    daw::rt::SpscQueue<int> queue { 4 };
    auto tx = queue.producer();
    auto rx = queue.consumer();

    int pushed = 0;
    while (tx.try_push (pushed))
        ++pushed;

    REQUIRE (pushed >= 4);                 // held at least the requested capacity
    REQUIRE_FALSE (tx.try_push (9999));    // still refuses (did not grow)

    // Everything that went in comes back out in order.
    for (int i = 0; i < pushed; ++i)
    {
        int out = -1;
        REQUIRE (rx.try_pop (out));
        REQUIRE (out == i);
    }
    int leftover = 0;
    REQUIRE_FALSE (rx.try_pop (leftover));
}

TEST_CASE ("SpscQueue: values cross threads in order", "[rt][spsc]")
{
    constexpr int count = 100'000;
    daw::rt::SpscQueue<int> queue { 1024 };

    auto tx = queue.producer();
    auto rx = queue.consumer();

    std::vector<int> received;
    received.reserve (count);

    std::thread consumer ([&]
    {
        int got = 0;
        while (received.size() < static_cast<std::size_t> (count))
        {
            if (rx.try_pop (got))
                received.push_back (got);
            // else: spin — producer hasn't caught up; never grows the queue.
        }
    });

    // Producer: keep retrying try_push on a full queue (back-pressure), never
    // falling back to an allocating enqueue.
    for (int i = 0; i < count; ++i)
        while (! tx.try_push (i))
            std::this_thread::yield();

    consumer.join();

    REQUIRE (received.size() == static_cast<std::size_t> (count));
    bool inOrder = true;
    for (int i = 0; i < count; ++i)
        if (received[static_cast<std::size_t> (i)] != i)
        {
            inOrder = false;
            break;
        }
    REQUIRE (inOrder);
}
