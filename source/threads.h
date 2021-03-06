/*---------------------------------------------
 *     modification time: 2016-07-24 08:00:45
 *     mender: Muse
-*---------------------------------------------*/

/*---------------------------------------------
 *     file: threads.h 
 *     creation time: 2016-07-11 12:57:45
 *     author: Muse 
-*---------------------------------------------*/

/*---------------------------------------------
 *       Source file content Five part
 *
 *       Part Zero:  Include
 *       Part One:   Define 
 *       Part Two:   Typedef
 *       Part Three: Struct
 *
 *       Part Four:  Function
 *
-*---------------------------------------------*/

/*---------------------------------------------
 *             Part Zero: Include
-*---------------------------------------------*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <signal.h>
#include <pthread.h>

#include <errno.h>

#include <time.h>
#include <sys/time.h>
#include <unistd.h>


/*---------------------------------------------
 *            Part Two: Typedef
-*---------------------------------------------*/

typedef pthread_mutex_t     mutex_t;
typedef pthread_cond_t      cond_t;
typedef pthread_barrier_t   barrier_t;
typedef pthread_t           thread_t;

typedef void                (*throutine)(void *);

typedef struct threadpool   Threads;
typedef struct threadentity Pthent;


/*---------------------------------------------
 *            Part Three: Struct
-*---------------------------------------------*/

struct threadpool {
    Pthent     *threads;
    Pthent     *freelist;

    thread_t    holder;

    mutex_t     free_lock;
    cond_t      free_cond;

    int32_t     cnt;
    barrier_t   barrier;

    /* signal mask */
    int32_t     how;
    sigset_t    sigset;
};

struct threadentity {
    thread_t    tid;

    Threads    *pool;
    Pthent     *next;

    throutine   routine;
    void       *params;

    barrier_t  *barrier;
    mutex_t     mutex;
    cond_t      cond;

    int32_t     flags;
};


/*---------------------------------------------
 *            Part Four: Function
-*---------------------------------------------*/

bool    mpc_create(Threads *pool, int num, int32_t how, sigset_t *sigset);
bool    mpc_thread_wake(Threads *pool, throutine func, void *params);
bool    mpc_thread_trywake(Threads *pool, throutine func, void *params);
bool    mpc_thread_wait(Threads *pool);
bool    mpc_thread_trywait(Threads *pool, struct timespec *abstime);
bool    mpc_destroy(Threads *pool);


