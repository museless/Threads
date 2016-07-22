#ifndef _MPCTL_H
#define _MPCTL_H

/*---------------------
    include
-----------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <pthread.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


/*---------------------
    define
-----------------------*/


/*---------------------
    typedef
-----------------------*/

typedef pthread_mutex_t     mutex_t;
typedef pthread_cond_t      cond_t;
typedef pthread_barrier_t   barrier_t;
typedef pthread_t           thread_t;

typedef void                (*throutine)(void *);

typedef struct threadpool   Threads;
typedef struct threadentity Pthent;


/*---------------------
    struct
-----------------------*/

struct pthread_pool {
    Pthent     *threads;
    Pthent     *free;

    int32_t     cnt;
    barrier_t   barrier;
};

struct threadentity {
    thread_t    tid;
    Pthent     *next_free;

    throutine   routine;
    void       *params;

    mutex_t     mutex;
    cond_t      cond;

    int32_t     flags;
};


/*---------------------
    glo fun
-----------------------*/

bool    mpc_create(Pthreads *pool, int numbers);
int     mpc_thread_wake(PTHPOOL *threadPool, pthrun runFun, void *pPara);
void    mpc_thread_wait(PTHPOOL *thPool);
void    mpc_destroy(PTHPOOL *thPool);


#endif
