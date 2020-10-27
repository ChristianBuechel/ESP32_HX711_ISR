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

#define N_WAVE 26 
#define DU 10   

extern "C"
{
	void app_main();
}


uint8_t i;

/*rmt_item32_t wave[N_WAVE];

for (i = 0; i < N_WAVE-2; ++i) 
{
	wave[i] = {{{DU, 1, DU, 0}}};
}
wave[N_WAVE-1] = {{{0, 1, 0, 0}}};
*/

rmt_item32_t wave25[] = { // use for strobing HX711
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{10, 1, 10, 0}}},	  // H for 1us and L for 1us (line is pulled down)
	{{{0, 1, 0, 0}}}};	  // RMT end marker

#define BUFF_PWR (2U)
#define BUFF_SIZE (1 << BUFF_PWR)
#define BUFF_SIZE_MASK (BUFF_SIZE - 1U)
struct buffer
{
	int32_t buff[BUFF_SIZE];
	uint8_t writeIndex = {0};
};
buffer weight_buffer; //FIFO holding past 16 values of temps in C * 1000

void write(struct buffer *buffer, int32_t value) //write to buffer
{
	//ESP_LOGI(TAG, "Writing chan %d, index %d Value : %d", chan, buffer->writeIndex[chan]& BUFF_SIZE_MASK, value);
	buffer->buff[(++buffer->writeIndex) & BUFF_SIZE_MASK] = value;
	//ESP_LOGI(TAG, "Index now %d", buffer->writeIndex[chan]& BUFF_SIZE_MASK);
}

int32_t readn(struct buffer *buffer, unsigned Xn) //read from buffer Xn=0 will read last written value
{
	return buffer->buff[(buffer->writeIndex - Xn) & BUFF_SIZE_MASK];
}

bool is_diff(int32_t a, int32_t b, int32_t range)
{
	return (a / range) != (b / range);
}

int32_t get_mean(struct buffer *buffer)
{
	int32_t sum_up = 0;
	for (i = 0; i < BUFF_SIZE; ++i)
	{
		sum_up += readn(buffer, i);
	}

	sum_up = sum_up >> BUFF_PWR; // divide by 2^BUFF_PWR for average
	return sum_up;				 // is now the average
}

void app_main(void)
{

	//setup tsic module returns the handle for the queue
	hx711_event_t hx711ev;
	QueueHandle_t hx711_events = hx711_init();

	uint32_t offset = 0; //needs updating
	uint32_t scale = 1;	 //needs updating

	rmt_fill_tx_items(RMT_TX_CHANNEL, wave25, sizeof(wave25) / sizeof(rmt_item32_t), 0);

	gpio_set_level(PD_SCK_PIN, 0); //enable chip

	while (1) //main loop
	{
		if (xQueueReceive(hx711_events, &hx711ev, 0)) //do not wait
		{
			//write(&weight_buffer, hx711ev.value);
			/*if (is_diff(readn(&weight_buffer, 0), readn(&weight_buffer, 1), 1000)) // only if diff by 100 print
			{
			ESP_LOGI(TAG, "New value            : %d", readn(&weight_buffer, 0));
			}*/
			ESP_LOGI(TAG, "Value              : %d", hx711ev.value);
            //ESP_LOGI(TAG, "Last reading(us)   : %d", uint32_t(hx711ev.time));
            //ESP_LOGI(TAG, "j                : %d", hx711ev.n_data);
			//ESP_LOGI(TAG, "New value            : %d", readn(&weight_buffer, 0));
			//ESP_LOGI(TAG, "Mean (%d samples)    : %d", BUFF_SIZE, get_mean(&weight_buffer));
            /*ESP_LOGI(TAG, "Last reading(us)   : %d", uint32_t(hx711ev.time));
            ESP_LOGI(TAG, "Number of RMT items  : %d", hx711ev.n_data);
            ESP_LOGI(TAG, "Cumulative duration  : %d", hx711ev.cumdur);
			float weight = ((float)hx711ev.value - offset) / scale; //hx711 value to g
            ESP_LOGI(TAG, "Weight (g)          : %f", weight);
            ESP_LOGI(TAG, "\n");*/
		}
		vTaskDelay(10 / portTICK_RATE_MS); //wait
	}
}
