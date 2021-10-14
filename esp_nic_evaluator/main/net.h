#ifndef __NET__
#define __NET__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "traffic.h"


#define NET_CORE            0
#define NET_STACK_SIZE      0x1000
#define NET_QUEUE_SIZE      0x400
#define NET_TASK_NAME       "net"
#define NET_TASK_PRIORITY   17
#define NET_PIN             GPIO_NUM_4
#define NET_PIN_MASK        (1ULL << NET_PIN)

#define NET_SOCK_MAX        255
#define NET_PORT_MAX        255
#define NET_BUF_SIZE        65535


/**
 * Network socket lookup table.
 *
 * @port    network port associated with socket
 * @task    reference to FreeRTOS task handle
 * @recv_cb receive callback
 */
typedef struct {
    unsigned short port;
    TaskHandle_t task;
    QueueHandle_t in_queue;
    void (*recv_cb)(void*, int, TaskHandle_t);
    void* dest_cb;
    int size_cb;
} sock_table_t;

/**
 * Network driver task struct
 *
 * @packet_queue    ingress packet queue
 * @task            FreeRTOS task handle
 * @tcb             FreeRTOS task tcb
 * @stack           stack area used by the task
 * @sock_table      socket to port and task mapping
 * @port_map        port to socket mapping
 * @sock_high       highest socket
 * @proto_data      source for packet data
 */
typedef struct {
    QueueHandle_t packet_queue;
    TaskHandle_t task;
    StaticTask_t tcb;
    StackType_t stack[NET_STACK_SIZE];
    sock_table_t sock_table[NET_SOCK_MAX];
    int port_map[NET_PORT_MAX];
    int sock_high;
} net_t;


/**
 * net_init() - Initializes and starts the network driver task.
 *
 * @net              network simulator configuration
 */
void net_init(net_t* net_ptr);


/**
 * net_gpio_isr() - ISR that simulates packet receiption.
 * @packet      a pointer to a pointer of a simulated packet `trace_packet_t`
 *
 * This is an ISR. It simulates a single packet by toggling a GPIO pin on/off.
 */
void IRAM_ATTR net_gpio_isr(void *packet);


#endif
