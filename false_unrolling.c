#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <time.h>
#include <assert.h>
#include <string.h>

/*
Without optimization (false sharing):

13,083,858 mem_load_l3_hit_retired_xsnp_hitm, 1.096439206 seconds time elapsed 
*/

/*
Using thread_func (array-based address translation):

910 mem_load_l3_hit_retired_xsnp_hitm, 0.692625282 seconds time elapsed
*/

/*
Using thread_func$n$ (manual unrolling):

748 mem_load_l3_hit_retired_xsnp_hitm, 0.521778744 seconds time elapsed
*/

struct timespec tpBegin2, tpEnd2;  // These are inbuilt structures to store the
                                   // time related activities
int *array;

const int NTHREAD = 4, NITER = 10000000;

#define PARTIAL  // Turns on partial unrolling when defined.

double compute(
    struct timespec start,
    struct timespec end)  // computes time in milliseconds given endTime and
                          // startTime timespec structures.
{
  double t;
  t = (end.tv_sec - start.tv_sec) * 1000;
  t += (end.tv_nsec - start.tv_nsec) * 0.000001;

  return t;
}

// thread_func$n$: Manual unrolling.
void *thread_func1(void *param) {
#ifdef PARTIAL
    for (int j = 0; j < NITER; j++)
        for (int i = 0; i < 20; i++)
            array[i]++;
#else
    for (int j = 0; j < NITER; j++)
        for (int i = 0; i < 20; i++)
            array[i]++;
#endif
    return NULL;
}

void *thread_func2(void *param) {
#ifdef PARTIAL
    for (int j = 0; j < NITER; j++)
        for (int i = 32; i < 52; i++)
            array[i]++;
#else
    for (int j = 0; j < NITER; j++)
        for (int i = 20; i < 40; i++)
            array[i]++;
#endif
    return NULL;
}

void *thread_func3(void *param) {
#ifdef PARTIAL
    for (int j = 0; j < NITER; j++)
        for (int i = 64; i < 84; i++)
            array[i]++;
#else
    for (int j = 0; j < NITER; j++)
        for (int i = 40; i < 60; i++)
            array[i]++;
#endif
    return NULL;
}

void *thread_func4(void *param) {
#ifdef PARTIAL
    for (int j = 0; j < NITER; j++)
        for (int i = 96; i < 116; i++)
            array[i]++;
#else
    for (int j = 0; j < NITER; j++)
        for (int i = 60; i < 80; i++)
            array[i]++;
#endif
    return NULL;
}

const int offsets[] = {0, 12, 24, 36};

//thread_func: array-based address translation.
void *thread_func(void *param) {
    int coef = (int)param;
#ifdef PARTIAL
    for (int j = 0; j < NITER; j++)
        for (int i = 20 * coef; i < 20 * (coef + 1); i++)
            array[i + offsets[coef]]++;
#else
    for (int j = 0; j < NITER; j++)
        for (int i = 20 * coef; i < 20 * (coef + 1); i++)
            array[i]++;
#endif
}

int main(int argc, char *argv[]) {
    array = (int*)malloc(400 * sizeof(int));
    int i;
    double time2;
    pthread_t threads[NTHREAD];

    //---------------------------START--------parallel computation with False
    //Sharing----------------------------

    clock_gettime(CLOCK_REALTIME, &tpBegin2);
    pthread_create(&threads[0], NULL, thread_func1, NULL);
    pthread_create(&threads[1], NULL, thread_func2, NULL);
    pthread_create(&threads[2], NULL, thread_func3, NULL);
    pthread_create(&threads[3], NULL, thread_func4, NULL);
    // for (i = 0; i < NTHREAD; i++)
    //     pthread_create(&threads[i], NULL, thread_func, (void*)i);
    for (i = 0; i < NTHREAD; i++) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_REALTIME, &tpEnd2);

    //---------------------------END----------parallel computation with False
    //Sharing----------------------------

    //--------------------------START------------------OUTPUT
    //STATS--------------------------------------------
    time2 = compute(tpBegin2, tpEnd2);
    printf("Time take with false sharing      : %f ms\n", time2);
    //--------------------------END------------------OUTPUT
    //STATS--------------------------------------------

    return 0;
}
