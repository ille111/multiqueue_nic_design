#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "tasks.h"
#include "worker.h"
#include "traffic.h"
#include "net_api.h"


static const char* TAG = "WRK";

extern unsigned int obs_cycles;
extern result_t *results;
extern obs_t *obs_times;
extern TaskHandle_t xObs;
/**
 * Track worker count.
 */
int worker_count = 0;

/**
 * worker_main() - processes work packages received via network port
 * @port    port to receive work data on
 *
 * The tasks running worker_main will generate cpu load proportional
 * to the amount of data that is received through the given port.
 */
static void worker_main(void* port);
static void observed_main(void* obs);

void
worker_init
(const char* worker_name, worker_t* wrk, int id, unsigned short port, int prio)
{
    int i_port = (int)port;
    task_worker[id] = &wrk->task;

    wrk->task = xTaskCreateStaticPinnedToCore(
        (TaskFunction_t)worker_main,
        worker_name,
        WORKER_STACK_SIZE,
        (void*)i_port,
        // WORKER_TASK_PRIORITY,
        prio,
        wrk->stack,
        &wrk->tcb,
        WORKER_CORE
    );

    worker_count++;
}

void
obs_init
(int prio, worker_t* wrk)
{
  // StackType_t stack[WORKER_STACK_SIZE];
  xObs = xTaskCreateStaticPinnedToCore(
      (TaskFunction_t)observed_main,
      "observed",
      WORKER_STACK_SIZE,
      (void*) 1,
      // WORKER_TASK_PRIORITY,
      prio,
      wrk->stack,
      &wrk->tcb,
      WORKER_CORE
  );
}

static unsigned int
binom
(unsigned int n, unsigned int k)
{
    if (2 * k > n) {
        k = n - k;
    }

    unsigned int result = 1;

    for (int i = 1; i <= k; i++) {
        result *= (n - k + i) / i;
    }

    return result;
}

void
load
()
{
    for (unsigned int i = 0; i <= 250; i++) {
        for (
            unsigned int j = 0;
            (i == 250 && j <= 150) || j <= i;
            j++
        ) {
            binom(i, j);
        }
    }
}

static void
worker_main
(void* port)
{
    int sock;
    unsigned int rando = 0;
    unsigned char buf[BUFFER_SIZE];

    ESP_LOGI(TAG, "Start worker in port=%d.", (int)port);
    ets_printf("Worker registered to core %d\n", xPortGetCoreID());

    /* Aquire socket from system. */
    if ((sock = net_sock()) <= 0) {
        ESP_LOGD(TAG, "Aquiring sock failed (%d).", sock);
    }

    /* Bind socket to port. */
    if ((net_bind(sock, (int)port)) < 0) {
        ESP_LOGD(TAG, "Port bind failed.");
    }

    /* Main worker loop */
    while (true) {
        /* Receive data from the 'network'. */
        net_recv(sock, buf, BUFFER_SIZE);

        /* Meassure time the packet took to get here. */
        long recv = esp_timer_get_time();

        /* Save that time to the results. */
        unsigned int seq = ((trace_packet_t*)buf)->seq;

        // ESP_LOGI(
        //     TAG, "rx=%ld, d=%hu, p=%u, c=%u (%d)",
        //     recv, ((trace_packet_t*)buf)->delta, ((trace_packet_t*)buf)->port, ((trace_packet_t*)buf)->count, ((trace_packet_t*)buf)->seq
        // );

        results[seq].received = recv;

        // /* Generate one random byte. */
        // rando = 0;
        // esp_fill_random(&rando, 1);
        //
        // /* Cut range. */
        // rando >>= WORKER_RAND_SHIFT;
        //
        // /* Scale. */
        // rando *= WORKER_RAND_BASE_US;
        //
        // /* Ensure minimum wait. */
        // rando += WORKER_RAND_MIN;
        //
        // ESP_LOGI(TAG, "seq=%d, recv=%ld, rando=%d", seq, recv, rando);

        /* Measure runtime over constant CPU time per packet. */
        long start_time = esp_timer_get_time();
        /* Busy wait. */
        load();
        unsigned int runtime = esp_timer_get_time() - start_time;
        results[seq].runtime = runtime;
    }
}

static void
observed_main
(void* obs)
{
    ets_printf("Observed task registered to core %d\n", xPortGetCoreID());
    int cnt = 0;
    vTaskSuspend(NULL);
    while(true){
        vTaskDelay(50/ portTICK_PERIOD_MS);
        long start_time = esp_timer_get_time();
        load();
        if(cnt == 0){
          unsigned int runtime = esp_timer_get_time() - start_time;
          obs_times[obs_cycles].runtime = runtime;
          obs_cycles++;
          cnt = 0;
        }
        //cnt++;
    }
}
