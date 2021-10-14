 /*
 * RXQ MUX
 */

#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "net.h"
#include "worker.h"
#include "traffic.h"


static const char* TAG = "RXQ_MUX_BOOT";


/*
 * Instanciate workers.
 */
static worker_t worker[4];
//static worker_t obs[1];
/*
 * Instanciate the traffic generator.
 */
static traffic_t traffic;

/*
 * Instanciate the network driver.
 */
static net_t net = {0};

static StaticTask_t obs_handle;

void app_main(void)
{
    ESP_LOGI(TAG, "app_main reached. Initializing ...");

    /* Initialize traffic. */
    traffic_init(&traffic);

    /* Initialize the network simulation. */
    net_init(&net);

    /* Initialize worker. */
    worker_init("WRK-1", &worker[0], 0, 0, 14);
    worker_init("WRK-2", &worker[1], 1, 1, 13);
    worker_init("WRK-3", &worker[2], 2, 2, 12);
    worker_init("WRK-4", &worker[3], 3, 3, 11);
    // worker_init("WRK-5", &worker[4], 4, 4, 10);
    //obs_init(15, &obs[0]);
}
