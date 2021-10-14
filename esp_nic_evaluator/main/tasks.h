#ifndef __TASKS__
#define __TASKS__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "worker.h"


TaskHandle_t *task_net;
TaskHandle_t *task_traffic;
TaskHandle_t *task_worker[WORKER_COUNT];

#endif
