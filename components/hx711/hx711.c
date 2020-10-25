
#include "hx711.h"

#define TAG "HX711_RMT"

static rmt_isr_handle_t s_rmt_driver_intr_handle;
int hx711_initialized = -1;

QueueHandle_t *hx711_queue;

static u_int16_t IRAM_ATTR rmt_get_mem_len(rmt_channel_t channel)
//this reoutine returns the number of items in RMT memory
{
    u_int16_t block_num = RMT.conf_ch[channel].conf0.mem_size;
    u_int16_t item_block_len = block_num * RMT_MEM_ITEM_NUM;
    volatile rmt_item32_t *data = RMTMEM.chan[channel].data32;
    u_int16_t idx;
    for (idx = 0; idx < item_block_len; idx++)
    {
        if (data[idx].duration0 == 0)
        {
            return idx;
        }
        else if (data[idx].duration1 == 0)
        {
            return idx + 1;
        }
    }
    return idx;
}

static void IRAM_ATTR hx711_interrupt(void *arg)
{
    uint32_t status;

    //evt.time = esp_timer_get_time();
    portBASE_TYPE HPTaskAwoken = pdFALSE;
    // 1) disable this interrupt
    gpio_set_intr_type(DOUT_PIN, GPIO_INTR_DISABLE);
    // 2) start RMT RX module

    //should be OK as it is initialized in init()

    // 3) start transmitting via RMT TX
    rmt_get_status(RMT_TX_CHANNEL, &status);
    if ((status & RMT_STATE_CH1) == 0)      //0 : idle, 1 : send, 2 : read memory, 3 : receive, 4 : wait
        rmt_tx_start(RMT_TX_CHANNEL, true); //send only if idle , if sending a new item while still sending screws everything up big time
    /* a problem might be that we don't know the precise interval between DOUT going low
       (ie firing this ISR) and the first pulse sent from the RMT
       but we can scope it  
    */

    //xQueueSendFromISR(triac_queue, &evt, &HPTaskAwoken);
    //could be useful for debugging

    if (HPTaskAwoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR rmt_isr_handler(void *arg)
// will fire once data is there and rmt times out
{
    uint32_t intr_st;
    uint8_t i,j;
    uint16_t n_data;
    hx711_event_t evt;
    //read RMT interrupt status.
    intr_st = RMT.int_st.val;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    RMT.conf_ch[RMT_RX_CHANNEL].conf1.rx_en = 0;
    RMT.conf_ch[RMT_RX_CHANNEL].conf1.mem_owner = RMT_MEM_OWNER_TX;

    /* First check the number of items (I guess 24 pulses max)
    CAVE each item in rmt_item32 are 2 pulses 
     */

    n_data = rmt_get_mem_len(RMT_RX_CHANNEL); //n pulses
    volatile rmt_item32_t *data = RMTMEM.chan[RMT_RX_CHANNEL].data32;

    //here we can decode the pulses to a value

    j = 0;
    uint32_t ref    = 0;
    uint32_t value  = 0; 
    uint32_t cumdur = 0;
    uint32_t start  = 174; // 164 pulses (16.4us) until RMT fires strobe
                           // then wait 1us and we should be in the middle of 1st strobe
    for (i = 0; i < n_data; i++) 
    {
        cumdur += data[i].duration0;
        ref = j*20+start;
        while (ref<cumdur)
        {
            //value |= (data[i].level0 << (24-j));
            value |= (data[i].level0 << (23-j));
            j++;
            ref = j*20+start;
        } 

        cumdur += data[i].duration1;
        //ref = j*20+start; not needed I guess
        while (ref<cumdur)
        {
            //value |= (data[i].level1 << (24-j));
            value |= (data[i].level1 << (23-j));
            j++;
            ref = j*20+start;
        } 
    }
       if (value & 0x800000)
        value |= 0xff000000;

	//value = value^0x800000;

    //make package ready for queue
    evt.time    = esp_timer_get_time(); //time stamp
    evt.cumdur  = cumdur;    //just a beginning
    evt.value   = value;     //just a beginning
    evt.n_data  = n_data;
    xQueueSendFromISR(hx711_queue, &evt, &HPTaskAwoken);

    RMT.conf_ch[RMT_RX_CHANNEL].conf1.mem_wr_rst = 1; //reset memory
    RMT.conf_ch[RMT_RX_CHANNEL].conf1.mem_owner = RMT_MEM_OWNER_RX;
    RMT.conf_ch[RMT_RX_CHANNEL].conf1.rx_en = 1; //enable RX

    //restore ISR status
    RMT.int_st.val = intr_st;
    // but clear bit for RX_receive int
    RMT.int_clr.ch0_rx_end = 1; //RMT channel 0 is RX
                                // dont forget to restart interrupt
    gpio_set_intr_type(DOUT_PIN, GPIO_INTR_NEGEDGE);

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

    //configure dout (data from hx711)
    gpio_config_t hx711;

    hx711.intr_type = GPIO_INTR_NEGEDGE;
    hx711.mode = GPIO_MODE_INPUT;
    hx711.pull_up_en = GPIO_PULLUP_ENABLE;
    hx711.pull_down_en = GPIO_PULLDOWN_DISABLE;
    hx711.pin_bit_mask = (1ULL << DOUT_PIN); //
    gpio_config(&hx711);

    //configure RMT for RX
    rmt_config_t config;
    config.channel = RMT_RX_CHANNEL;
    config.gpio_num = DOUT_PIN;
    config.rmt_mode = RMT_MODE_RX;
    config.mem_block_num = 1; // use default of 1 memory block
    config.clk_div = 8;       //0.1 us resolution
    config.rx_config.filter_en = true;
    config.rx_config.filter_ticks_thresh = 10; //only pulses longer than 5us
    config.rx_config.idle_threshold = 6000;    // 600us
                                               //readout takes about max 27 * 2 us = 54 us
                                               //we use the 10 SPS mode, i a sample every 100ms
                                               //using 200us works
                                               //UPDATE: scoped it and ???
    ESP_ERROR_CHECK(rmt_config(&config));
    rmt_set_rx_intr_en(config.channel, true);
    rmt_isr_register(rmt_isr_handler, NULL, ESP_INTR_FLAG_LEVEL1, &s_rmt_driver_intr_handle);
    rmt_rx_start(config.channel, 1);
    ESP_LOGI(TAG, "Installed RMT RX driver\n");

    //configure GPIO for RMT TX
    hx711.intr_type = GPIO_INTR_DISABLE;
    hx711.mode = GPIO_MODE_OUTPUT;
    hx711.pull_up_en = GPIO_PULLUP_DISABLE;
    hx711.pull_down_en = GPIO_PULLDOWN_ENABLE;
    hx711.pin_bit_mask = (1ULL << PD_SCK_PIN); //
    gpio_config(&hx711);

    //and disable chip for now
    gpio_set_level(PD_SCK_PIN, 1);

    //configure RMT for TX
    config.channel = RMT_TX_CHANNEL;
    config.gpio_num = PD_SCK_PIN;
    config.mem_block_num = 1; // use default of 1 memory block
    config.tx_config.carrier_en = false;
    config.tx_config.loop_en = false;
    config.tx_config.idle_output_en = true; //idle_out_lv interesting as its sets the idle level
    config.rmt_mode = RMT_MODE_TX;
    //config.clk_div = 80; //start with 1us res later change to 8 = 0.1 us resolution
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_LOGI(TAG, "Installed RMT TX driver 0.\n");

    //add interrupt to DOUT
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(DOUT_PIN, hx711_interrupt, (void *)DOUT_PIN);
    ESP_LOGI(TAG, "Enabled DOUT interrupt.\n");

    hx711_initialized = 1;
    return hx711_queue;
}
