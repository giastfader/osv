/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// condvar is OSv's implemenation of the classic "condition variable"
// synchronization primitive. It is similar to condition variables in POSIX
// Threads, but makes one additional guarantees that POSIX Threads do not:
// Our condvar_wait() is guaranteed not to have "spurious wakeups", i.e.,
// condvar_wait() will only return if someone called condvar_wake_one()/all().
//
// The difference is subtle. Usually you still need to check the condition
// after condvar_wait() returns because other threads may race us to change
// the condition. But in some cases we use this guarantee. One example is our
// semaphore implementation (semaphore.cc): In the traditional implementation
// when spurious condvar_wait wakeups are possible, the wakee decrements the
// semaphore's counter (this can cause a "thundering herd" problem). Using
// our condvar without spurious wakeups, it is possible to decrement the
// counter in the waker, and decide exactly who to wake.
//
// So watch out - if this implementation is ever rewritten, it should continue
// to guarantee no-spurious-wakeups - even if POSIX Threads doesn't need it.

#ifndef CONDVAR_H_
#define CONDVAR_H_

#include <stdint.h>

#include <osv/mutex.h>

#ifdef __cplusplus

// While the condvar type can be used in C, to C++ code we offer additional
// convenience methods and functions, for which we need these headers:
#include "sched.hh"

#endif

// The "wait morphing" feature requires mutex->send_lock() to be available
// and that the condvar structure can be enlarged with another field.
// Currently neither will work in the old spin-based mutex implementation.
#ifdef LOCKFREE_MUTEX
#define WAIT_MORPHING 1
#endif


// Note: To be useful for implementing pthread's condition variables, the
// condvar_t structure doesn't need any special initialization beyond zero
// initialization (note PTHREAD_COND_INITIALIZER is all zeros). For this
// to work, mutex_t which we use below, should also be ok with zero
// initialization - and in the current implementation it indeed is.
//
// Moreover, we also don't have a de-initialization function, as there
// is no memory dynamically allocated, nor is there anything meaningful
// to do when the waiter list is not empty.

struct wait_record;

typedef struct condvar {
    mutex_t m;
    struct {
        // A FIFO queue of waiters - a linked list from oldest (next in line
        // to be woken) towards newest. The wait records themselves are held
        // on the stack of the waiting thread - so no dynamic memory
        // allocation is needed for this list.
        struct wait_record *oldest;
        struct wait_record *newest;
    } waiters_fifo;
#if WAIT_MORPHING
    // Remember mutex last used in a wait(), for use in "wait morphing"
    // feature. We disallow (as Posix Threads do) using different mutexes in
    // concurrent wait()s on the same condvar. We could lift this requirement,
    // but then we would need to remember the user_mutex on each wait_record.
    mutex_t *user_mutex;
#endif

#ifdef __cplusplus
    // In C++, for convenience also provide methods.
    condvar() { memset(this, 0, sizeof *this); }
    inline int wait(mutex_t *user_mutex, sched::timer *tmr = nullptr);
    inline int wait(mutex_t &user_mutex, sched::timer *tmr = nullptr);
    inline void wake_one();
    inline void wake_all();
    template <class Pred>
    void wait_until(mutex& mtx, Pred pred);
#endif
} condvar_t;

#define CONDVAR_INITIALIZER	{}


#ifdef __cplusplus
extern "C" {
#endif

int condvar_wait(condvar_t *condvar, mutex_t* user_mutex, uint64_t expiration);
void condvar_wake_one(condvar_t *condvar);
void condvar_wake_all(condvar_t *condvar);

#ifdef __cplusplus
}

// additional convenience functions for C++
int condvar_wait(condvar_t *condvar, mutex_t *user_mutex, sched::timer *tmr);
int condvar_t::wait(mutex_t *user_mutex, sched::timer *tmr) {
    return condvar_wait(this, user_mutex, tmr);
}
int condvar_t::wait(mutex_t &user_mutex, sched::timer *tmr) {
    return condvar_wait(this, &user_mutex, tmr);
}
void condvar_t::wake_one() {
    return condvar_wake_one(this);
}
void condvar_t::wake_all() {
    return condvar_wake_all(this);
}

template <class Pred>
inline
void condvar::wait_until(mutex& mtx, Pred pred)
{
    while (!pred()) {
        wait(&mtx);
    }
}


#endif

#endif /* MUTEX_H_ */
