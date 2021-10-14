#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "xtensa/core-macros.h"

#include "tasks.h"
#include "traffic.h"
#include "trace.h"


static const char* TAG = "TFC";


extern int worker_count;

/**
 * Store results of the experiment.
 */
result_t *results;
obs_t *obs_times;
unsigned int obs_cycles = 0;
TaskHandle_t xObs = NULL;

/**
 * traffic_trace_reader() - trace converter worker loop
 *
 * Reads in packets from the provided trace blob, triggers transmission
 * simulation of packet via gpio toggle and measures the transmission start.
 */
static void traffic_trace_reader(raw_trace_packet_t* trace);

/**
 * traffic_send_packet() - trigger transmission simulation of a packet
 *
 * A gpio will be toggled in order to trigger an isr that enqueues a packet.
 */
static void traffic_send_packet(void);

/**
 * traffic_check_done() - check whether the trace and the worker are done
 *
 * The worker might take significant longer time than the traffic generator.
 */
static int traffic_check_done(void);

/**
 * traffic_print_results() - print the results to serial
 *
 * A function that transmit the results in csv format via the serial line.
 */
static void traffic_print_results(void);

/**
 * traffic_gpio_init() - initialize traffic generator gpios
 *
 * Applies pin configurations for gpio pins.
 */
static void traffic_gpio_init(void);


/**
 * Internal edge tracking variable.
 */
static uint32_t __edge;

/**
 * Configure the TRAFFIC_PIN that conveys the packet signal.
 */
static gpio_config_t traffic_gpio_cfg = {
    .mode = GPIO_MODE_OUTPUT,
    .intr_type = GPIO_PIN_INTR_DISABLE,
    .pin_bit_mask = TRAFFIC_PIN_MASK,
    .pull_up_en = 0,
    .pull_down_en = 0
};


void
traffic_init
(traffic_t* t)
{
    /* Init shared space. */
    memset(&shared, 0, sizeof(trace_packet_t));

    /* Convert trace. */
    raw_trace_packet_t* raw_packet_trace = (raw_trace_packet_t*)trace;

    /* Aquire space for the results. */
    results = (result_t*)malloc(sizeof(result_t) * TRACE_PACKET_COUNT);
    obs_times = (obs_t*)malloc(sizeof(obs_t) * 1000);
    /* Initialize gpio & timer. */
    __edge = 0;
    traffic_gpio_init();

    /* Traffec geeration to CPU TRAFFIC_CORE. */
    t->task = NULL;
    task_traffic = &t->task;

    t->task = xTaskCreateStaticPinnedToCore(
        (TaskFunction_t)traffic_trace_reader,
        TRAFFIC_TASK_NAME,
        TRAFFIC_STACK_SIZE,
        raw_packet_trace,
        configMAX_PRIORITIES-1,
        t->stack,
        &t->tcb,
        TRAFFIC_CORE
    );
}

static void
IRAM_ATTR
traffic_trace_reader
(raw_trace_packet_t* trace)
{
    int lol = 0;
    static uint32_t notified = 1;
    long sent = 0;
    long notify_delay = 0;

    ESP_LOGI(
        TAG, "Trace consists of %d packets in %d irqs.",
        TRACE_PACKET_COUNT, TRACE_IRQ_COUNT
    );
    ets_printf("Traffic generator registered to core %d\n", xPortGetCoreID());
    /* Let the other tasks get ready. */
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* Start Observed Task */
    // vTaskResume(xObs);

    /* Loop through the irqs of the trace. */
    for (int i = 0; i < TRACE_IRQ_COUNT; i++) {

        /* Wait indefinetely till the ISR stops using the previous packet. */
        if (notified) {

            /* Make it available to the ISR to be enqueued in the driver. */
            shared.port = trace[i].port;
            shared.delta = trace[i].delta;
            shared.count = trace[i].count;

            /* Sleep for the specified duration. */
            lol += shared.delta;
            ESP_LOGI(TAG, "delta => %hu, sum => %d", shared.delta, lol);

            long remaining_delta = shared.delta - notify_delay;
            if(remaining_delta > 0){
                ets_delay_us(remaining_delta);
                // ets_delay_us(1000);
            }//else{
            //  ets_printf("Warning: Previous ISR not fast enough by %ld us", remaining_delta);
            //}

            /* Measure and save transmission start time for packets in IRQ. */
            sent = esp_timer_get_time();
            for(int k=0; k<shared.count; k++) {
                results[shared.seq + k].sent = sent;
            }

            /* Wiggle the gpio pin. */
            traffic_send_packet();

            // ESP_LOGI(
            //     TAG, "sq=%d, tx=%ld, d=%hu, p=%u, c=%u (%d)",
            //     i, sent, shared.delta, shared.port, shared.count, shared.seq
            // );
        }

        /* Wait for the packet being put into the ingress queue. */
        notified = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        notify_delay = esp_timer_get_time() - sent;
    }

    /* Wait an arbitrary amount of time before write out results. */
    while (!traffic_check_done())
        vTaskDelay(100 / portTICK_PERIOD_MS);

    /* Worker grace time. */
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* Stop Observed Task */
    // vTaskSuspend(xObs);

    /* Print results to serial line. */
    traffic_print_results();

    /* Do nothing for ever. For ever? For ever ever. */
    while (true)
        vTaskDelay(10000 / portTICK_PERIOD_MS);
}

static void
traffic_send_packet
(void)
{
    if (__edge) {
        gpio_set_level(TRAFFIC_PIN, 0);
        __edge = 0;
    } else {
        gpio_set_level(TRAFFIC_PIN, 1);
        __edge = 1;
    }
}

static int
traffic_check_done
(void)
{
    int i = TRACE_PACKET_COUNT - worker_count;

    for (;i < TRACE_PACKET_COUNT - 1; i++) {
        if (!results[i].received) {
            return 0;
        }
    }

    return 1;
}

static void
traffic_print_results
(void)
{
    ets_printf("seq, sent, recv, tx_delay, runtime\n");
    for(int i = 0; i < TRACE_PACKET_COUNT; i++) {
        unsigned int tx_delay = results[i].received - results[i].sent;
        ets_printf(
            "%d, %u, %u, %u, %u\n",
            i, results[i].sent, results[i].received, tx_delay, results[i].runtime
        );
    }
    ets_printf("END\n");
    // for(int i = 0; i < obs_cycles; i++) {
    //     ets_printf("%d: %u\n", i, obs_times[i].runtime);
    // }
    // ets_printf("END\n");
}

static void
traffic_gpio_init
(void)
{
    /* Apply config for TRAFFIC_PIN */
    gpio_config(&traffic_gpio_cfg);
}
