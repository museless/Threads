/*---------------------------------------------
 *     modification time: 2016-07-24 15:10:21
 *     mender: Muse
-*---------------------------------------------*/

/*---------------------------------------------
 *     file: example.c
 *     creation time: 2016-07-12 18:29:21
 *     author: Muse
-*---------------------------------------------*/

#include "threads.h"


void routine(void *param)
{
    //while (true)
        ;   /* nothing */
}


void run(void *param)
{
    printf("I want to fuck you\n");
    printf("Wait there\n");
}


int main(void)
{
    Threads pool;

    if (!mpc_create(&pool, 4)) {
        perror("mpc_create");
        return  -1;
    }

    if (!mpc_thread_wake(&pool, routine, NULL)) {
        perror("mpc_thread_wake");
        return  -1;
    }

    if (!mpc_thread_wake(&pool, run, NULL)) {
        perror("mpc_thread_wake");
        return  -1;
    }

    if (!mpc_thread_wait(&pool))
        perror("mpc_thread_wait");

    mpc_destroy(&pool);

    return  -1;
}
