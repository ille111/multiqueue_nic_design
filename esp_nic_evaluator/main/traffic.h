#ifndef __TRAFFIC__
#define __TRAFFIC__

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"


#define TRAFFIC_PIN               GPIO_NUM_18
#define TRAFFIC_PIN_MASK          (1ULL << TRAFFIC_PIN)
#define TRAFFIC_CORE              1
#define TRAFFIC_TASK_NAME         "traffic"
#define TRAFFIC_STACK_SIZE        0x1000
#define TRAFFIC_TIMER_SCALE       40000000
#define TRAFFIC_TIMER_INITIAL     (10 * TRAFFIC_TIMER_SCALE)


/**
 * struct traffic_t - traffic task configuration struct
 * @task            FreeRTOS task handle
 * @tcb             Reference to the FreeRTOS task tcb
 * @stack           Actual stack of the task.
 *
 * `traffic_t` configures the traffic module and stores structures and
 * handles of the related components.
 *
 */
typedef struct traffic_t traffic_t;

struct traffic_t {
    TaskHandle_t task;
    StaticTask_t tcb;
    StackType_t stack[TRAFFIC_STACK_SIZE];
};


/**
 * struct raw_trace_packet_t - raw trace packet model struct
 * @delta           time between two packets in choses resolution
 * @port            target port of the packet
 *
 * `raw_trace_packet_t` boils down the packet to the target port which i
 * mapped to a process and the time it arrives relative to the previous packet.
 * For the purpose of this experiment 256 different ports are enough. The
 * delta value has enough values available since it can be scaled anyway.
 *
 */
typedef struct raw_trace_packet_t raw_trace_packet_t;

struct __attribute__((__packed__)) raw_trace_packet_t {
    unsigned int delta;
    unsigned char port;
    unsigned char count;
};

/**
 * struct trace_packet_t - trace packet struct
 * @seq             packet sequence number
 * @delta           time between two packets in choses resolution
 * @port            target port of the packet
 *
 * Additionally to the values of `raw_trace_packet_t` a sequence number
 * can be assigned during the processing of the raw packet trace.
 */
typedef struct trace_packet_t trace_packet_t;

struct __attribute__((__packed__)) trace_packet_t {
    unsigned int seq;
    unsigned int delta;
    unsigned char port;
    unsigned char count;
};

/**
 * struct trace_packet_t - trace packet struct
 * @sent            time when the packet has been sent
 * @received        time the worker took the packet from his queue
 *
 * Sending text over the serial line takes a lot of time and processing power
 * therefore the data is stores as a result and sent when the simulation has
 * finished.
 */
typedef struct result_t result_t;

struct __attribute__((__packed__)) result_t {
    unsigned int sent;
    unsigned int received;
    unsigned int runtime;
};

typedef struct obs_t obs_t;

struct __attribute__((__packed__)) obs_t {
    unsigned int runtime;
};

/**
 * traffic_init() - initialize and start trace to traffic conversion.
 *
 * the traffic module initializes a task that configures a timer interrupt
 * repeatedly based on the time delta of the next packet in the trace blob..
 */
void traffic_init(traffic_t* t);

/**
 * traffic_timer_isr() - Responsible for toggling correct GPIOs.
 * @id      Receive the interrupt identifier.
 *
 * This is an ISR. It simulates a single packet by toggling the correct  GPIO
 * pin on/off based on the underlying trace.
 */
void IRAM_ATTR traffic_timer_isr(void *id);


/*
 * Shared pointer used to pass data between net and traffic objects.
 */
trace_packet_t shared;

#endif
