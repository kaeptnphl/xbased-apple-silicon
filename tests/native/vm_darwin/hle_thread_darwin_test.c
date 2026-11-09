#include "hw/xbox/hle/util/thread.h"

#include <assert.h>
#include <pthread.h>

static void *post_after_delay(void *arg)
{
    HleSem *sem = arg;
    struct timespec delay = {.tv_nsec = 10000000};
    nanosleep(&delay, NULL);
    hle_sem_post(sem);
    return NULL;
}

int main(void)
{
    HleSem sem;
    hle_sem_init(&sem, 0);

    assert(!hle_sem_timedwait(&sem, 5));

    pthread_t poster;
    assert(pthread_create(&poster, NULL, post_after_delay, &sem) == 0);
    assert(hle_sem_timedwait(&sem, 1000));
    assert(pthread_join(poster, NULL) == 0);
    assert(!hle_sem_trywait(&sem));

    hle_sem_post(&sem);
    assert(hle_sem_trywait(&sem));
    assert(!hle_sem_trywait(&sem));
    hle_sem_destroy(&sem);

    HleSem sem2;
    hle_sem_init(&sem2, 2);
    assert(hle_sem_trywait(&sem2));
    assert(hle_sem_trywait(&sem2));
    assert(!hle_sem_trywait(&sem2));
    hle_sem_destroy(&sem2);

    return 0;
}
