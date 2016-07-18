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
static  void   *mpc_thread_pool_run(void *thread_para);
static  void    mpc_thpool_run_prepration(Pthent *entity);
static  void    mpc_thread_self_exit(Pthent *entity);

/* Part Six */
static  void    mpc_wait_clock(int microsec);
static  Pthent *mpc_search_empty(PTHPOOL *thread_pool);
static  int     mpc_thread_create(Pthent *entity);
static  void    mpc_thread_cleanup(void *thread_para);

static  void    mpc_thread_join(Pthent *th_entity);

/* Part Seven */
static  void    mpc_thread_perror(char *errStr, int nErr);
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
bool mpc_create(PTHPOOL *pool, int pthread_num)
{
    if (pthread_num < 1) {
        errno = EINVAL;
        return  NULL;
    }

    pool->pl_cnt = pthread_num;

    if (!(pool->pl_list = malloc(pool->pl_cnt * sizeof(Pthent)))) {
        mpc_destroy(pool);
        return  NULL;
    }

    /* create pthread: in pool */
    Pthent *entity = pool->pl_list;

    for (int count_num = 0; count_num < pool->pl_cnt; count_num++, entity++) {
        if (!mpc_thread_create(entity)) {
            mpc_destroy(pool);
            return  NULL;
        }
    }

    return  pool;
}


/*-----mpc_thread_wake-----*/
int mpc_thread_wake(PTHPOOL *threadPool, pthrun runFun, void *pPara)
{
    Pthent *pentity;

    if (!(pentity = mpc_search_empty(threadPool)))
        return  PTH_RUN_END;

    pentity->pe_run = runFun;
    pentity->pe_data = pPara;
    pentity->pe_flags = PTH_IS_BUSY;

    pthread_cond_signal(&pentity->pe_cond);
    pthread_mutex_unlock(&pentity->pe_mutex);

    return  PTH_RUN_OK;
}


/*-----mpc_thread_wait-----*/
void mpc_thread_wait(PTHPOOL *thread_pool)
{
    Pthent *thList;

    if (thread_pool && thread_pool->pl_list) {
        /* pass the watcher thread */
        for (thList = thread_pool->pl_list + 1;
             thList < thread_pool->pl_list + thread_pool->pl_cnt; 
             thList++) {
            pthread_mutex_lock(&thList->pe_mutex);

            while (thList->pe_flags == PTH_IS_BUSY)
                pthread_cond_wait(&thList->pe_cond, &thList->pe_mutex);

            pthread_mutex_unlock(&thList->pe_mutex);
        }
    }
}


/*-----mpc_destroy-----*/
void mpc_destroy(PTHPOOL *thread_pool)
{
    Pthent *th_entity;
    int     status;

    if (!thread_pool)
        return;

    if (thread_pool->pl_list) {
        th_entity = thread_pool->pl_list;

        for (; th_entity < thread_pool->pl_list + thread_pool->pl_cnt; th_entity++) {
            if (th_entity->pe_flags != PTH_WAS_KILLED) {
                if (!pthread_tryjoin_np(th_entity->pe_tid, NULL))
                    continue;

                if (th_entity->pe_flags == PTH_IS_BUSY) {
                    mpc_wait_clock(MILLISEC_500);

                    status = pthread_mutex_trylock(&th_entity->pe_mutex);

                    if (status == EBUSY && th_entity->pe_flags == PTH_IS_BUSY) {
                        mpc_thread_join(th_entity);
                        continue;
                    }
                }

                th_entity->pe_flags = PTH_ALREADY_KILLED;
                pthread_cond_signal(&th_entity->pe_cond);
            }
        }

        free(thread_pool->pl_list);
    }

    free(thread_pool);
}


/*----------------------------------------------
 *  Part Five: Associated with thread pool run
 *
 *      1. mpc_thread_pool_run
 *      2. mpc_thpool_run_preparation
 *      3. mpc_thread_self_exit
 *
**----------------------------------------------*/

/*-----mpc_thread_pool_run-----*/
void *mpc_thread_pool_run(void *thread_para)
{
    Pthent  *th_entity = (Pthent *)thread_para;

    mpc_thpool_run_prepration(th_entity);

    while (PTH_RUN_PERMANENT) {
        pthread_mutex_lock(&th_entity->pe_mutex);

        while (th_entity->pe_flags != PTH_IS_BUSY) {
            if (th_entity->pe_flags == PTH_ALREADY_KILLED) 
                mpc_thread_self_exit(th_entity);

            pthread_cond_wait(&th_entity->pe_cond, &th_entity->pe_mutex);
        }

        /* routine running */
        pthread_cleanup_push(mpc_thread_cleanup, th_entity);
        th_entity->pe_run(th_entity->pe_data);
        pthread_cleanup_pop(0);

        th_entity->pe_flags = PTH_IS_READY;

        pthread_cond_signal(&th_entity->pe_cond);
        pthread_mutex_unlock(&th_entity->pe_mutex);
    }
}


/*-----mpc_thpool_run_prepration-----*/
void mpc_thpool_run_prepration(Pthent *entity)
{
    int status;

    if ((status = pthread_mutex_lock(&entity->pe_mutex)))
        mpc_thread_therror(entity, "pthread_mutex_lock", status);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    if ((status = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL)))
        mpc_thread_therror(entity, "pthread_setcanceltype", status);

    entity->pe_flags = PTH_IS_READY;

    if ((status = pthread_cond_signal(&entity->pe_cond)))
        mpc_thread_therror(entity, "pthread_cond_signal", status);

    if ((status = pthread_mutex_unlock(&entity->pe_mutex)))
        mpc_thread_therror(entity,
            "mpc_thpool_run_prepration - pthread_mutex_unlock", status);
}


/*-----mpc_thread_self_exit-----*/
void mpc_thread_self_exit(Pthent *entity)
{
    pthread_mutex_unlock(&entity->pe_mutex);

    pthread_cond_destroy(&entity->pe_cond);
    pthread_mutex_destroy(&entity->pe_mutex);

    pthread_detach(pthread_self());
    pthread_exit(NULL);
}


/*-----------------------------------------------
 *          Part Six: Other part
 *
 *          1. mpc_wait_clock
 *          2. mpc_search_empty
 *          3. mpc_thread_create
 *          4. mpc_thread_cleanup
 *          6. mpc_thread_join
 *
**-----------------------------------------------*/

/*-----mpc_wait_clock-----*/
void mpc_wait_clock(int microsec)
{
    TMVAL   thread_clock;

    thread_clock.tv_sec = 0;
    thread_clock.tv_usec = microsec;
    select(0, NULL, NULL, NULL, &thread_clock);
}


/*-----mpc_search_empty-----*/
Pthent *mpc_search_empty(PTHPOOL *thread_pool)
{
    Pthent *thread_list_end, *entity;
    int     status;

    thread_list_end = thread_pool->pl_list + thread_pool->pl_cnt;
    entity = thread_pool->pl_list;

    for (; entity < thread_list_end; entity++) {
        if (entity->pe_flags == PTH_IS_READY) {
            if ((status = 
                 pthread_mutex_trylock(&entity->pe_mutex))) {

                if (status == EBUSY)
                    continue;

                return  NULL;
            }

            return  entity;
        }
    }

    return  NULL;
}


/*-----mpc_thread_create-----*/
int mpc_thread_create(Pthent *entity)
{
    int32_t status;

    entity->pe_data = NULL;
    entity->pe_run = NULL;
    entity->flags = PTH_IS_UNINITED;

    if ((status = pthread_mutex_init(&entity->pe_mutex, NULL)) ||
        (status = pthread_cond_init(&entity->pe_cond, NULL))) {
        errno = status;
        return  false;
    }

    status = pthread_create(&entity->tid, NULL, mpc_thread_run, (void *)entity);

    if (status) {
        errno = status;
        return  false;
    }

    /* wait for initing done */
    if ((status = pthread_mutex_lock(&entity->pe_mutex))) {
        errno = status;
        return  false;
    }

    while (entity->flags != PTH_IS_READY) {
        if ((status = pthread_cond_wait(&entity->cond, &entity->mutex))) {
            errno = status;
            return  false;
        }
    }

    if ((status = pthread_mutex_unlock(&entity->mutex))) {
        errno = status
        return  false;
    }

    return  true;
}


/*-----mpc_thread_cleanup-----*/
void mpc_thread_cleanup(void *thread_para)
{
    Pthent  *entity = (Pthent *)thread_para;

    pthread_mutex_unlock(&entity->pe_mutex);
    entity->pe_flags = PTH_WAS_KILLED;
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
 *      1. mpc_thread_perror
 *      2. mpc_thread_therror
 *
_*---------------------------------------------*/

/*-----mpc_thread_perror-----*/
void mpc_thread_perror(char *errStr, int nErr)
{
    printf("ThreadPool---> %s - %s\n", errStr, strerror(nErr));
}


/*-----mpc_thread_therror-----*/
void mpc_thread_therror(Pthent *entity, char *errStr, int nErr)
{
    mpc_thread_perror(errStr, nErr);

    entity->pe_flags = PTH_WAS_KILLED;
    
    pthread_detach(entity->pe_tid);
    pthread_cond_signal(&entity->pe_cond);
    pthread_mutex_unlock(&entity->pe_mutex);

    pthread_exit(NULL);
}



