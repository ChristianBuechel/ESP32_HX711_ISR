/* 
An example of how to read the HX 711 op amp with a load cell attcahed using the ESP32  
It's running completely in the background and an interrupt
is fired once new data has arrived. Data is transmitted to the main code 
via a queue 
Tare with 't', get weight with 'w', set scaling factor with 's;real_weight'
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hx711.h"
#include "SerialCommand.h"
#include "uart.h"

#define TAG "MAIN"

extern "C"
{
	void app_main();
}

uint32_t offset = 0; //needs updating
float scale = 1;	 //needs updating

uint8_t i;
float tared_mean;
SerialCommand myCMD; // The  SerialCommand object

#define BUFF_PWR (3U)
#define BUFF_SIZE (1 << BUFF_PWR)
#define BUFF_SIZE_MASK (BUFF_SIZE - 1U)
struct buffer
{
	int32_t buff[BUFF_SIZE];
	uint8_t writeIndex = {0};
};
buffer weight_buffer; //FIFO holding past 2^BUFF_PWR values

void write(struct buffer *buffer, int32_t value) //write to buffer
{
	buffer->buff[(++buffer->writeIndex) & BUFF_SIZE_MASK] = value;
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

void process_t() //tare
{
	offset = get_mean(&weight_buffer);
}

void process_w() //get weight
{
			ESP_LOGI(TAG, "Mean raw (%d samples)    : %d", BUFF_SIZE, get_mean(&weight_buffer));
			tared_mean = (float)get_mean(&weight_buffer)-offset;
			ESP_LOGI(TAG, "Mean (tared)             : %f", tared_mean );
			ESP_LOGI(TAG, "Weight (g)               : %2.2f", tared_mean/scale);
}
void process_s() //set scale factor
{
	char *arg;
	arg = myCMD.next(); // Get the next argument from the SerialCommand object buffer
	if (arg != NULL)	// As long as it exists, take it
	{
		scale = atof(arg);
		tared_mean = (float)get_mean(&weight_buffer)-offset;
		scale = tared_mean/scale;
		ESP_LOGI(TAG, "Set scale to %f%%", scale);
	}
}

void unrecognized(const char *command)
{
	char *arg;
	arg = myCMD.next(); // Get the next argument from the SerialCommand object buffer
	if (arg != NULL)	// As long as it exists, take it
	{
	    ESP_LOGI(TAG, "> %s;%s", command,arg);
	}
	else
	{
	    ESP_LOGI(TAG, "> %s", command);
	}
}

void app_main(void)
{
	//setup hx711 module returns the handle for the queue
	hx711_event_t hx711ev;
	QueueHandle_t hx711_events = hx711_init();

	gpio_set_level(PD_SCK_PIN, 0); //enable chip

	//initialite UART
	uart_init();
	myCMD.setDefaultHandler(unrecognized);	//
	myCMD.addCommand("s", process_s); // register parse handlers
	myCMD.addCommand("t", process_t); // register parse handlers
	myCMD.addCommand("w", process_w); // register parse handlers
	myCMD.clearBuffer();

	while (1) //main loop
	{
		myCMD.readSerial();
		if (xQueueReceive(hx711_events, &hx711ev, 0)) //do not wait
		{
			write(&weight_buffer, hx711ev.value);
		}
		vTaskDelay(10 / portTICK_RATE_MS); //wait
	}
}
