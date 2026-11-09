/*
 * POSIX timing compatibility for Darwin's relative condition waits and
 * missing clock_nanosleep API. Timed condition deadlines in this project are
 * CLOCK_MONOTONIC values.
 */
#ifndef NATIVE_COMPAT_DARWIN_POSIX_H
#define NATIVE_COMPAT_DARWIN_POSIX_H

#include <errno.h>
#include <pthread.h>
#include <time.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

static inline int nc_darwin_pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock)
{
    (void)attr;
    return clock == CLOCK_MONOTONIC || clock == CLOCK_REALTIME ? 0 : EINVAL;
}

static inline int nc_darwin_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                                                   const struct timespec *monotonic_deadline)
{
    struct timespec monotonic_now;
    if (clock_gettime(CLOCK_MONOTONIC, &monotonic_now) != 0) {
        return errno;
    }

    struct timespec remaining = {
        .tv_sec = monotonic_deadline->tv_sec - monotonic_now.tv_sec,
        .tv_nsec = monotonic_deadline->tv_nsec - monotonic_now.tv_nsec,
    };
    if (remaining.tv_nsec < 0) {
        remaining.tv_sec--;
        remaining.tv_nsec += 1000000000L;
    }
    if (remaining.tv_sec < 0) {
        remaining = (struct timespec){0};
    }

    return pthread_cond_timedwait_relative_np(cond, mutex, &remaining);
}

static inline int nc_darwin_clock_nanosleep(clockid_t clock, int flags, const struct timespec *request,
                                           struct timespec *remaining)
{
    if (!request || (flags & ~TIMER_ABSTIME)) {
        return EINVAL;
    }
    if (!(flags & TIMER_ABSTIME)) {
        return nanosleep(request, remaining) == 0 ? 0 : errno;
    }

    struct timespec now;
    if (clock_gettime(clock, &now) != 0) {
        return errno;
    }
    struct timespec delay = {
        .tv_sec = request->tv_sec - now.tv_sec,
        .tv_nsec = request->tv_nsec - now.tv_nsec,
    };
    if (delay.tv_nsec < 0) {
        delay.tv_sec--;
        delay.tv_nsec += 1000000000L;
    }
    if (delay.tv_sec < 0) {
        delay = (struct timespec){0};
    }
    return nanosleep(&delay, remaining) == 0 ? 0 : errno;
}

static inline int nc_darwin_pthread_setname_np(pthread_t thread, const char *name)
{
    return pthread_equal(thread, pthread_self()) ? pthread_setname_np(name) : ENOTSUP;
}

#define pthread_condattr_setclock nc_darwin_pthread_condattr_setclock
#define pthread_cond_timedwait nc_darwin_pthread_cond_timedwait
#define clock_nanosleep nc_darwin_clock_nanosleep
#define pthread_setname_np nc_darwin_pthread_setname_np

#endif
