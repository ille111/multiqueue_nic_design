#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "net.h"
#include "tasks.h"
#include "traffic.h"


static const char* TAG = "NET";


/**
 * Local Reference to net instance.
 */
static net_t *net;

/**
 * Configure the NET_PIN that receives the packet signal.
 */
static gpio_config_t net_gpio_cfg = {
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_PIN_INTR_ANYEDGE,
    .pin_bit_mask = NET_PIN_MASK,
    .pull_up_en = 1,
};

/**
 * net_main() - net driver er task
 *
 * Acts as the main loop that dequeues an item from the
 * general receive queue on each cycle.
 */
static void net_main(net_t* net);

/**
 * net_process_packet() - Simulates load per packet
 *
 * distributes packet among subscribed tasks.
 */
static void net_process_packet(net_t* net, trace_packet_t* packet);

/**
 * net_recv_cb() - network data receive callback function
 *
 * This callback function is installed into the sock_table when a user
 * wants to receive data on that particular socket.
 */
static void net_recv_cb(void *dest, int size, QueueHandle_t handle);


void
net_init
(net_t* net_ptr)
{
    /* Store a local reference in this object. */
    net = net_ptr;

    /* Create Shared data objects and init sock table.. */
    net->packet_queue = xQueueCreate(NET_QUEUE_SIZE, sizeof(trace_packet_t));
    net->sock_high = 0;
    ets_printf("ISR registered to core %d\n", xPortGetCoreID());
    /* Apply config for NET_PIN. */
    gpio_config(&net_gpio_cfg);

    /* Set IRQ to trigger on rising edge. */
    gpio_set_intr_type(NET_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);

    /* Pass ISR callback */
    gpio_isr_handler_add(NET_PIN, net_gpio_isr, (void*)NET_PIN);

    net->task = NULL;
    task_net = &net->task;
    net->task = xTaskCreateStaticPinnedToCore(
        (TaskFunction_t)net_main,
        NET_TASK_NAME,
        NET_STACK_SIZE,
        net,
        NET_TASK_PRIORITY,
        net->stack,
        &net->tcb,
        NET_CORE
    );
}

void IRAM_ATTR
net_gpio_isr
(void *id)
{
    BaseType_t queue_woke = pdFALSE;
    BaseType_t notify_woke = pdFALSE;

    if ((int)id == NET_PIN) {

        /*
         * Simulate packet reception. Batch receive all packages
         * that belong to the same IRQ which are determined by the
         * interrupt moderation trace from which the trace is generated.
         */
        for(int i=0; i<shared.count; i++) {
            xQueueSendFromISR(
                net->packet_queue,
                &shared,
                &queue_woke
            );
            shared.seq++;
        }

        /* Unblock trace reader. */
        vTaskNotifyGiveFromISR(*task_traffic, &notify_woke);

        /* Force reschedule after ISR. */
        portYIELD_FROM_ISR(&notify_woke);
    }
}

int
net_sock
(void)
{
    /* Increment sock mark. */
    net->sock_high++;

    /* Associate sock with task. */
    sock_table_t *entry = &net->sock_table[net->sock_high];
    entry->task = xTaskGetCurrentTaskHandle();
    entry->in_queue = xQueueCreate(NET_QUEUE_SIZE, sizeof(trace_packet_t));
    if (entry->in_queue == NULL)
        ESP_LOGD(TAG, "Queue creation failed.");

    return net->sock_high;
}


int
net_bind
(int sock, unsigned short port)
{
    sock_table_t *entry = &net->sock_table[sock];

    /* Unassociated sock or taken by another task. */
    if (entry->task == NULL || entry->task != xTaskGetCurrentTaskHandle())
        return -1;

    /* Port out of range. */
    if (port > NET_PORT_MAX)
        return -2;

    /* Associate sock with port and populate port map. */
    entry->port = port;
    net->port_map[port] = sock;

    return 0;
}


int
net_recv
(int sock, void* dest, int size)
{
    /*
     * NOTE:
     * For the pupose of the project callbacks do not need to be removed
     * once installed in the sock_table. In a real setting the callback
     * must be removed, otherwise it would be executed arbitrary times (each
     * time data arrives) and result in data loss.
     */

    sock_table_t *entry = &net->sock_table[sock];

    /* Unassociated sock or taken by another task. */
    if (entry->task == NULL || entry->task != xTaskGetCurrentTaskHandle())
        return -1;

    /* Install callback. */
    entry->recv_cb = net_recv_cb;
    entry->dest_cb = dest;

    /* Only allow NET_BUF_SIZE amount of bytes to be copied. */
    if (0 < size && size < NET_BUF_SIZE) {
        entry->size_cb = size;
    } else {
        return -2;
    }

    /* Wait for the driver to aquire data on that port. */
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

    /* Execute receive callback. */
    entry->recv_cb(entry->dest_cb, entry->size_cb, entry->in_queue);

    return size;
}

static void
net_main
(net_t* net)
{
    ets_printf("NET registered to core %d\n", xPortGetCoreID());
    while (1) {
        trace_packet_t packet;

        /* Receive packet from trace or block. */
        BaseType_t status = xQueueReceive(
            net->packet_queue,
            &packet,
            (TickType_t)portMAX_DELAY
        );

        /* Process package. */
        if (status == pdTRUE) {
            net_process_packet(net, &packet);
        }
    }
}

static void
net_process_packet
(net_t* net, trace_packet_t* packet)
{
    ESP_LOGD(
        TAG, "PP seq=%d, delta=%hu, port=%u",
        packet->seq, packet->delta, packet->port
    );

    /* Find process to notify about ingress data. */
    int sock = net->port_map[packet->port];

    if (0 < sock && sock < NET_SOCK_MAX) {
        /* Put the packet into the socket mailbox. */
        if(xQueueSend(net->sock_table[sock].in_queue, packet, 0) == pdPASS) {
            /* Notify the associated task. */
            xTaskNotifyGive(net->sock_table[sock].task);
        } else {
            ESP_LOGD(TAG, "Queue full for that socket.");
        }
    } else {
        ESP_LOGD(TAG, "No sock registered for that port (:%d).", packet->port);
    }
}

static void
net_recv_cb
(void *dest, int size, QueueHandle_t queue)
{
    /* Receive packet. */
    xQueueReceive(queue, dest, 0);

    /*
     * Note:
     * In reality src would be taken from the network device.
     * Some smaller embedded chips do not offer RXDMA queues.
     */

    char *ptr = (char*)dest;
    ptr += sizeof(trace_packet_t);
    size -= sizeof(trace_packet_t);

    for (int i=0; i<size; i++) {
        *ptr++ = '\0';
    }
}
