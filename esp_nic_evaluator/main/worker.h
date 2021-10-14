#ifndef __WORKER__
#define __WORKER__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WORKER_CORE             0
#define WORKER_COUNT            4
#define WORKER_STACK_SIZE       0x1000
#define WORKER_TASK_PRIORITY    10
#define BUFFER_SIZE             512

#define WORKER_RAND_MIN         10
#define WORKER_RAND_SHIFT       4
#define WORKER_RAND_BASE_US     10
#define BUSYTIME                5000

/**
 * worker_t - worker task struct
 *
 * @task            FreeRTOS task handle
 * @tcb             FreeRTOS task tcb
 * @stack           stack area used by the task
 */
typedef struct {
    TaskHandle_t task;
    StaticTask_t tcb;
    StackType_t stack[WORKER_STACK_SIZE];
} worker_t;


/**
 * worker_init() - Initializes and starts a worker thread.
 *
 * @worker_name     port to receive work from
 * @worker          worker configuration struct
 * @id              identifier of the worker, used to mask to tasks.h handles
 * @port            port to receive work from
 * @prio            priority of worker task
 *
 * TODO: Clean up the messy interface.
 */
void worker_init(const char* worker_name, worker_t* worker,
                                        int id, unsigned short port, int prio);

void obs_init(int prio, worker_t* worker);


#endif
