#ifndef _HX711_H_
#define _HX711_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/rmt.h"

#include "esp_log.h"
#include <stdint.h>

#define ESP_INTR_FLAG_DEFAULT 0

// HX711 data pin configuration 
#define DOUT_PIN   GPIO_NUM_18
#define PD_SCK_PIN GPIO_NUM_19

    typedef struct
    {
        uint64_t time;   // time of measurement
        int32_t value;  // 24 bit value of HX711
    } hx711_event_t;

    QueueHandle_t *hx711_init();

#ifdef __cplusplus
}
#endif

#endif