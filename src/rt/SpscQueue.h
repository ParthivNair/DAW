// Lock-free single-producer / single-consumer queue — the canonical UI<->audio
// messaging primitive for the whole project.
//
// Thin wrapper over moodycamel::ReaderWriterQueue. The point of the wrapper is
// API SAFETY, not new behaviour: the RT side must never be able to reach the
// allocating `enqueue` / capacity-growing path. We enforce that by handing out
// two distinct, move-only views onto one queue:
//
//   * Producer  — for the thread that WRITES. Only exposes try_push (the
//                 non-allocating, fails-when-full path). The allocating
//                 enqueue() is simply not a member, so it cannot be called.
//   * Consumer  — for the thread that READS. Only exposes try_pop / peek.
//
// Either side may be the audio thread; both `try_push` and `try_pop` are
// RT_NONBLOCKING. The queue is pre-sized at construction; nothing on the
// RT-facing API ever allocates.
//
// Typical wiring (one of the two ends lives on the audio thread):
//     rt::SpscQueue<Msg> q { 256 };
//     auto tx = q.producer();   // hand to the writer thread
//     auto rx = q.consumer();   // hand to the reader thread
//
// See docs/realtime-rules.md.
#pragma once

#include <cstddef>
#include <utility>

#include <readerwriterqueue.h>

#include "rt/RTAnnotations.h"

namespace daw::rt
{
/** Writer-side view of an SpscQueue. Move-only, non-owning; valid only while
    the owning SpscQueue is alive. Exposes ONLY non-allocating operations, so the
    allocating enqueue path is unreachable from here by construction. */
template <typename T>
class SpscProducer
{
public:
    /** Pushes a copy. Returns false (without growing) when the queue is full.
        Never allocates — safe on the audio thread. */
    bool try_push (const T& value) noexcept RT_NONBLOCKING { return queue->try_enqueue (value); }

    /** Pushes by move. Returns false (without growing) when the queue is full.
        Never allocates — safe on the audio thread. */
    bool try_push (T&& value) noexcept RT_NONBLOCKING { return queue->try_enqueue (std::move (value)); }

private:
    template <typename> friend class SpscQueue;
    explicit SpscProducer (moodycamel::ReaderWriterQueue<T>* q) noexcept : queue (q) {}

    moodycamel::ReaderWriterQueue<T>* queue;
};

/** Reader-side view of an SpscQueue. Move-only, non-owning; valid only while the
    owning SpscQueue is alive. Exposes ONLY non-allocating operations. */
template <typename T>
class SpscConsumer
{
public:
    /** Pops the front element into `out`. Returns false when empty. Never
        allocates — safe on the audio thread. */
    bool try_pop (T& out) noexcept RT_NONBLOCKING { return queue->try_dequeue (out); }

    /** Returns a pointer to the front element without removing it, or nullptr
        when empty. The pointer is valid until the next try_pop. */
    T* peek() noexcept RT_NONBLOCKING { return queue->peek(); }

    /** Drops the front element. Returns false when empty. Pair with peek(). */
    bool pop() noexcept RT_NONBLOCKING { return queue->pop(); }

private:
    template <typename> friend class SpscQueue;
    explicit SpscConsumer (moodycamel::ReaderWriterQueue<T>* q) noexcept : queue (q) {}

    moodycamel::ReaderWriterQueue<T>* queue;
};

/** Owns a pre-sized lock-free SPSC queue and hands out one producer and one
    consumer view. The underlying buffer is allocated ONCE here, at construction;
    the views expose only non-allocating, fails-when-full operations.

    Single-producer / single-consumer: call producer() from exactly one writer
    thread and consumer() from exactly one reader thread. */
template <typename T>
class SpscQueue
{
public:
    /** Constructs a queue guaranteed to hold at least `capacity` elements
        without further allocation. Allocates here, on the constructing
        (non-RT) thread — never on the RT path. */
    explicit SpscQueue (std::size_t capacity) : queue (capacity) {}

    SpscQueue (const SpscQueue&) = delete;
    SpscQueue& operator= (const SpscQueue&) = delete;

    /** The writer-side view. Use from a single producer thread. */
    SpscProducer<T> producer() noexcept { return SpscProducer<T> { &queue }; }

    /** The reader-side view. Use from a single consumer thread. */
    SpscConsumer<T> consumer() noexcept { return SpscConsumer<T> { &queue }; }

    /** Approximate element count. Safe to call from either thread; for
        diagnostics only (the exact size races with the other thread). */
    std::size_t size_approx() const noexcept RT_NONBLOCKING { return queue.size_approx(); }

private:
    moodycamel::ReaderWriterQueue<T> queue;
};
} // namespace daw::rt
