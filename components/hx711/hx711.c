
#include "hx711.h"

#define TAG "HX711_ISR"

int hx711_initialized = -1;

QueueHandle_t *hx711_queue;

static void IRAM_ATTR hx711_interrupt(void *arg)
{
    uint32_t data = 0;
    hx711_event_t evt;

    portBASE_TYPE HPTaskAwoken = pdFALSE;
    // 1) disable this interrupt
    gpio_set_intr_type(DOUT_PIN, GPIO_INTR_DISABLE);
    evt.time = esp_timer_get_time(); //time stamp

    // 2) strobe data
    for (size_t i = 0; i < 24; i++)
    {
        gpio_set_level(PD_SCK_PIN, 1);
        ets_delay_us(1);
        data |= gpio_get_level(DOUT_PIN) << (23 - i);
        gpio_set_level(PD_SCK_PIN, 0);
        ets_delay_us(1);
    }
        gpio_set_level(PD_SCK_PIN, 1);//25th strobe for config
        ets_delay_us(1);
        gpio_set_level(PD_SCK_PIN, 0);
        ets_delay_us(1);

 if (data & 0x800000)
        data |= 0xff000000;

    evt.value = (int32_t)data;
    xQueueSendFromISR(hx711_queue, &evt, &HPTaskAwoken);
    gpio_set_intr_type(DOUT_PIN, GPIO_INTR_NEGEDGE); // dont forget to restart interrupt
    if (HPTaskAwoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

QueueHandle_t *hx711_init()
{
    if (hx711_initialized != -1)
    {
        ESP_LOGI(TAG, "Already initialized");
        return NULL;
    }

    hx711_queue = xQueueCreate(10, sizeof(hx711_event_t));
    ESP_LOGI(TAG, "HX711 queue created");

    //configure GPIO for DOUT (data from hx711)
    gpio_config_t hx711;
    hx711.intr_type = GPIO_INTR_NEGEDGE;
    hx711.mode = GPIO_MODE_INPUT;
    hx711.pull_up_en = GPIO_PULLUP_ENABLE;
    hx711.pull_down_en = GPIO_PULLDOWN_DISABLE;
    hx711.pin_bit_mask = (1ULL << DOUT_PIN); //
    gpio_config(&hx711);

    //configure GPIO for PD_SCK
    hx711.intr_type = GPIO_INTR_DISABLE;
    hx711.mode = GPIO_MODE_OUTPUT;
    hx711.pull_up_en = GPIO_PULLUP_DISABLE;
    hx711.pull_down_en = GPIO_PULLDOWN_ENABLE;
    hx711.pin_bit_mask = (1ULL << PD_SCK_PIN); //
    gpio_config(&hx711);

    //and disable chip for now
    gpio_set_level(PD_SCK_PIN, 1);

    //add interrupt to DOUT
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(DOUT_PIN, hx711_interrupt, (void *)DOUT_PIN);
    ESP_LOGI(TAG, "Enabled DOUT interrupt.\n");

    hx711_initialized = 1;
    return hx711_queue;
}
