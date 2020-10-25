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

#define RMT_RX_CHANNEL RMT_CHANNEL_0 //channel 0 
#define RMT_TX_CHANNEL RMT_CHANNEL_1 //channel 1 

// HX711 data pin configuration 
#define DOUT_PIN   GPIO_NUM_18
#define PD_SCK_PIN GPIO_NUM_19
#define RMT_RX_PIN GPIO_NUM_21


    typedef struct
    {
        uint64_t time;   // time of measurement
        uint64_t result0; // 24 bit value of HX711
        uint64_t result1; // 24 bit value of HX711
        uint16_t n_data; // number of pulses received
    } hx711_event_t;

    QueueHandle_t *hx711_init();

#ifdef __cplusplus
}
#endif

#endif