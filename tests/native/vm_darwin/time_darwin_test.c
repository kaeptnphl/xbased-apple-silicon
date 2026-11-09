#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
    pthread_condattr_t attr;
    pthread_cond_t cond;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    if (pthread_condattr_init(&attr) != 0 ||
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0 ||
        pthread_cond_init(&cond, &attr) != 0) {
        fputs("monotonic condition setup failed\n", stderr);
        return 1;
    }
    pthread_condattr_destroy(&attr);

    struct timespec wait_start, wait_end;
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    wait_start = deadline;
    deadline.tv_nsec += 20000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    pthread_mutex_lock(&mutex);
    const int wait_result = pthread_cond_timedwait(&cond, &mutex, &deadline);
    pthread_mutex_unlock(&mutex);
    clock_gettime(CLOCK_MONOTONIC, &wait_end);
    if (wait_result != ETIMEDOUT) {
        fputs("monotonic condition wait did not time out\n", stderr);
        return 2;
    }
    if (wait_end.tv_sec < deadline.tv_sec ||
        (wait_end.tv_sec == deadline.tv_sec && wait_end.tv_nsec < deadline.tv_nsec)) {
        fputs("monotonic condition wait returned before its deadline\n", stderr);
        return 5;
    }
    const double wait_seconds = (double)(wait_end.tv_sec - wait_start.tv_sec) +
                                (double)(wait_end.tv_nsec - wait_start.tv_nsec) / 1000000000.0;
    if (wait_seconds > 2.0) {
        fputs("monotonic condition wait exceeded its timeout tolerance\n", stderr);
        return 6;
    }

    struct timespec start, end, sleep_until;
    clock_gettime(CLOCK_MONOTONIC, &start);
    sleep_until = start;
    sleep_until.tv_nsec += 20000000L;
    if (sleep_until.tv_nsec >= 1000000000L) {
        sleep_until.tv_sec++;
        sleep_until.tv_nsec -= 1000000000L;
    }
    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleep_until, NULL) != 0) {
        fputs("absolute monotonic sleep failed\n", stderr);
        return 3;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (end.tv_sec < sleep_until.tv_sec ||
        (end.tv_sec == sleep_until.tv_sec && end.tv_nsec < sleep_until.tv_nsec)) {
        fputs("absolute monotonic sleep returned before its deadline\n", stderr);
        return 4;
    }

    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
    puts("Darwin monotonic condition and absolute sleep passed");
    return 0;
}
