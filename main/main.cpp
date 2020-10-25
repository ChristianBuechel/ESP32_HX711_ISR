/* 
An example of how to read the HX 711 op amp with a load cell attcahed using the ESP32 RMT 
engine. It's running completely in the background and an interrupt
is fired once new data has arrived. Data is transmitted to the main code 
via a queue 
*/
/*#include <stdio.h>
#include "freertos/task.h"
#include "esp_spi_flash.h"
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "esp_system.h"

#include "hx711.h"

#define TAG "MAIN"

extern "C"
{
    void app_main();
}


rmt_item32_t wave25[] = { // use for strobing HX711
//	{{{10, 0, 10, 0}}},     // keep L for 2us (because of ISR latency prob not needed)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},     // H for 1us and L for 1us (line is pulled down)
	{{{0, 1, 0, 0}}}}; 	    // RMT end marker


void app_main(void)
{

    //setup tsic module returns the handle for the queue
    hx711_event_t hx711ev;
    QueueHandle_t hx711_events = hx711_init();

    uint32_t offset = 0; //needs updating
    uint32_t scale  = 1; //needs updating


	rmt_fill_tx_items(RMT_TX_CHANNEL, wave25, sizeof(wave25) / sizeof(rmt_item32_t), 0);
    //wake up HX711
    //gpio_set_level(PD_SCK_PIN, 0);

    //enable chip
    gpio_set_level(PD_SCK_PIN, 0);
    //rmt_tx_start(RMT_TX_CHANNEL, true);
    // and fire for once 
    while (1) //main loop
    {
        if (xQueueReceive(hx711_events, &hx711ev, 0)) //do not wait
        {            
            ESP_LOGI(TAG, "Last reading(us)    : %d", uint32_t(hx711ev.time));
            ESP_LOGI(TAG, "Number of RMT items : %d", hx711ev.n_data);
            ESP_LOGI(TAG, "Value               : %d", uint32_t(hx711ev.result));
            float value = ((float)hx711ev.result - offset) / scale; //hx711 value to g
            ESP_LOGI(TAG, "Weight (g)          : %f", value);
            ESP_LOGI(TAG, "\n");        
        }
        vTaskDelay(10 / portTICK_RATE_MS); //wait
    }
}
