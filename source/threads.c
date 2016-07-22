/*---------------------------------------------
 *  modification time: 2016.07.18 20:10
 *  creator: Muse
 *  mender: Muse
 *  intro: pthread pool
-*---------------------------------------------*/

/*---------------------------------------------
 *      Source file content Eight part
 *
 *      Part Zero:  Include
 *      Part One:   Define
 *      Part Two:   Local data
 *      Part Three: Local function
 *
 *      Part Four:  Muse thpool API
 *      Part Five:  Associated with thread pool run
 *      Part Six:   Other part 
 *      Part Seven: Thread pool error handle
 *
-*---------------------------------------------*/

/*---------------------------------------------
 *           Part Zero: Include
-*---------------------------------------------*/

#include "threads.h"


/*---------------------------------------------
 *           Part Two: Local data
-*---------------------------------------------*/

enum PTHREAD_STATE {
    PTH_IS_WATCHER,
    PTH_IS_UNINITED,
    PTH_ALREADY_KILLED,
    PTH_WAS_KILLED,
    PTH_IS_READY,
    PTH_IS_BUSY
};


/*---------------------------------------------
 *         Part Three: Local function
-*---------------------------------------------*/

/* Part Five */
static  void   *_thread_routine(void *thread_para);
static  void    _thread_prepare(Pthent *entity);

/* Part Six */
static  void    _time_wait(int microsec);
static  Pthent *_empty_slot(Threads *thread_pool);
static  int     _thread_create(Pthreads *pool, Pthent *thread);
static  void    _thread_cleanup(void *thread_para);

static  void    mpc_thread_join(Pthent *th_entity);

/* Part Seven */
static  void    mpc_thread_therror(Pthent *entity, char *errStr, int nErr);


/*---------------------------------------------
 *      Part Four: Muse thpool API
 *
 *          1. mpc_create
 *          2. mpc_thread_wake
 *          3. mpc_thread_wait
 *          4. mpc_destroy
 *
-*---------------------------------------------*/

/*-----mpc_create------*/
bool mpc_create(Pthreads *pool, int numbers)
{
    if (!pool || numbers < 1) {
        errno = EINVAL;
        return  false;
    }

    if ((errno = pthread_barrier_init(&pool->barrier, NULL, numbers + 1)))
        return  false;

    pool->free = NULL;
    pool->cnt = numbers;

    if (!(pool->threads = malloc(pool->cnt * sizeof(Pthent)))) {
        mpc_destroy(pool);
        return  false;
    }

    Pthent *thread = pool->threads;

    for (; thread < pool->threads + pool->cnt; thread++) {
        if (!_thread_create(pool, thread)) {
            mpc_destroy(pool);
            return  false;
        }
    }

    pthread_barrier_wait(&pool->barrier);

    return  true;
}


/*-----mpc_thread_wake-----*/
bool mpc_thread_wake(Threads *pool, pthrun func, void *params)
{
    if (!pool) {
        errno = EINVAL;
        return  false;
    }

    Pthent *thread;

    if (!(thread = _empty_slot(pool)))
        return  false;

    thread->routine = func;
    thread->params = params;
    thread->flags = PTH_IS_BUSY;

    pthread_cond_signal(&thread->pe_cond);
    pthread_mutex_unlock(&thread->pe_mutex);

    return  true;
}


/*-----mpc_thread_wait-----*/
void mpc_thread_wait(Threads *thread_pool)
{
    Pthent *thList;

    if (thread_pool && thread_pool->threads) {
        /* pass the watcher thread */
        for (thList = thread_pool->threads + 1;
             thList < thread_pool->threads + thread_pool->pl_cnt; 
             thList++) {
            pthread_mutex_lock(&thList->pe_mutex);

            while (thList->flags == PTH_IS_BUSY)
                pthread_cond_wait(&thList->pe_cond, &thList->pe_mutex);

            pthread_mutex_unlock(&thList->pe_mutex);
        }
    }
}


/*-----mpc_destroy-----*/
void mpc_destroy(Threads *pool)
{
    Pthent *th_entity;
    int     status;

    if (!pool) {
        errno = EINVAL;
        return;
    }

    if (pool->threads) {
        th_entity = pool->threads;

        for (; th_entity < pool->threads + pool->pl_cnt; th_entity++) {
            if (th_entity->flags != PTH_WAS_KILLED) {
                if (!pthread_tryjoin_np(th_entity->pe_tid, NULL))
                    continue;

                if (th_entity->flags == PTH_IS_BUSY) {
                    _time_wait(MILLISEC_500);

                    status = pthread_mutex_trylock(&th_entity->pe_mutex);

                    if (status == EBUSY && th_entity->flags == PTH_IS_BUSY) {
                        mpc_thread_join(th_entity);
                        continue;
                    }
                }

                th_entity->flags = PTH_ALREADY_KILLED;
                pthread_cond_signal(&th_entity->pe_cond);
            }
        }

        free(pool->threads);
    }

    pthread_barrier_destroy(&pool->barrier);
}


/*----------------------------------------------
 *  Part Five: Associated with thread pool run
 *
 *      1. _thread_routine
 *      2. _thread_prepare
 *
**----------------------------------------------*/

/*-----_thread_routine-----*/
void *_thread_routine(void *thread_para)
{
    Pthent  *th_entity = (Pthent *)thread_para;

    _thread_prepare(th_entity);

    while (true) {
        pthread_mutex_lock(&th_entity->mutex);

        while (th_entity->flags != PTH_IS_BUSY) {
            if (th_entity->flags == PTH_ALREADY_KILLED) 
                pthread_exit(NULL);

            pthread_cond_wait(&th_entity->cond, &th_entity->mutex);
        }

        th_entity->routine(th_entity->params);
        th_entity->flags = PTH_IS_READY;

        pthread_cond_signal(&th_entity->cond);
        pthread_mutex_unlock(&th_entity->mutex);
    }
}


/*-----_thread_prepare-----*/
bool _thread_prepare(Pthent *entity)
{
    if (entity->flags == PTH_IS_UNINITED) {
        if ((errno = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
            (errno = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL)))
            return  false;

        pthread_cleanup_push(_thread_cleanup, entity);
        entity->flags = PTH_IS_READY;
        pthread_barrier_wait(&entity->barrier);
    }

    return  true;
}


/*-----------------------------------------------
 *          Part Six: Other part
 *
 *          1. _time_wait
 *          2. _empty_slot
 *          3. _thread_create
 *          4. _thread_cleanup
 *          6. mpc_thread_join
 *
**-----------------------------------------------*/

/*-----_time_wait-----*/
void _time_wait(int microsec)
{
    TMVAL   thread_clock;

    thread_clock.tv_sec = 0;
    thread_clock.tv_usec = microsec;
    select(0, NULL, NULL, NULL, &thread_clock);
}


/*-----_empty_slot-----*/
Pthent *_empty_slot(Threads *pool)
{
    if (pool->free) {
        Pthent  *thread = pool->free;

        pool->free = thread->next_free;
        thread->next_free = NULL;

        return  thread;
    }

    return  NULL;
}


/*-----_thread_create-----*/
bool _thread_create(Pthreads *pool, Pthent *thread)
{
    thread->routine = thread->params = NULL;
    thread->flags = PTH_IS_UNINITED;

    thread->next_free = pool->free;
    pool->free = thread;

    if ((errno = pthread_mutex_init(&thread->mutex, NULL)) ||
        (errno = pthread_cond_init(&thread->cond, NULL)))
        return  false;

    errno = pthread_create(&entity->tid, NULL, _thread_routine, thread);

    return  (errno) ? false : true;
}


/*-----_thread_cleanup-----*/
void _thread_cleanup(void *thread_para)
{
    Pthent  *entity = (Pthent *)thread_para;

    pthread_mutex_unlock(&entity->mutex);
    
    pthread_mutex_destroy(&entity->mutex);
    pthread_cond_destroy(&entity->cond);

    entity->flags = PTH_WAS_KILLED;
}


/*-----mpc_thread_join-----*/
void mpc_thread_join(Pthent *th_entity)
{
    pthread_cancel(th_entity->pe_tid);

    pthread_mutex_unlock(&th_entity->pe_mutex);
    pthread_cond_destroy(&th_entity->pe_cond);

    pthread_join(th_entity->pe_tid, NULL);
}


/*---------------------------------------------
 *    Part Seven: Thread pool error handle
 *
 *      1. mpc_thread_therror
 *
-*---------------------------------------------*/

/*-----mpc_thread_therror-----*/
void mpc_thread_therror(Pthent *entity, char *errStr, int nErr)
{
    mpc_thread_perror(errStr, nErr);

    entity->flags = PTH_WAS_KILLED;
    
    pthread_detach(entity->pe_tid);
    pthread_cond_signal(&entity->pe_cond);
    pthread_mutex_unlock(&entity->pe_mutex);

    pthread_exit(NULL);
}



