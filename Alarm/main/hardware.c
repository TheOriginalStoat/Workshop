// https://github.com/esp-idf-lib/ina3221

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include <wiegand.h>
#include "program.h"
#include "config.h"

#define WIFI_RETRY  5
#define GPIO_OUTPUT_IO_0 18
#define GPIO_OUTPUT_IO_1 19
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0 20
#define GPIO_INPUT_IO_1 21
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0
#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock IO1*/
#define ESP_INTR_FLAG_DEFAULT 0
#define BIT_BUTTON_CHANGED BIT(0)
#define WARNING_CHANNEL 1
#define WARNING_CURRENT (40.0)

//#define STRUCT_SETTING 0
#if defined EXAMPLE_MEASURING_MODE_TRIGGER
#define MODE false  // true : continuous  measurements // false : trigger measurements
#else
#define MODE true
#endif
static EventGroupHandle_t eg = NULL;
static EventGroupHandle_t wifi_event_group;
static QueueHandle_t gpio_evt_queue = NULL;
static wiegand_reader_t reader;
static QueueHandle_t queue = NULL;

typedef struct
{
    uint8_t data[4]; // CONFIG_EXAMPLE_BUF_SIZE
    size_t bits;
} data_packet_t;

static void IRAM_ATTR gpio_isr_handler(void* arg);
static void gpio_task(void* arg);
void test(void *pvParameters);
void set_pin(uint8_t pin, bool state);

bool active_wifi = false;
static const char *TAG_wifi = "wifi";
static const char *TAG_gpio = "gpio";
static const char *TAG_sensors = "sensors";
static const char *TAG_wiegand = "wiegand";
const char *wifi_ssid;
const char *wifi_pass;
const int CONNECTED_BIT = BIT0;
const int FAIL_BIT = BIT1;
int s_retry_num = 0;
char msg[128];

void showESPHardware(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);

    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) 
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

void sensors_read(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(sensors_read_interval / portTICK_PERIOD_MS);
        ESP_LOGV(TAG_sensors, "Read sensors");

      }
}

static void IRAM_ATTR intr_handler(void *arg)
{
    // On interrupt set bit in event group
    BaseType_t hp_task;
    if (xEventGroupSetBitsFromISR(eg, BIT_BUTTON_CHANGED, &hp_task) != pdFAIL)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        portYIELD_FROM_ISR(hp_task);
#else
        portYIELD_FROM_ISR();
#endif
}

#pragma region Wiegand
static void reader_callback(wiegand_reader_t *r)
{
    // you can decode raw data from reader buffer here, but remember:
    // reader will ignore any new incoming data while executing callback

    // create simple undecoded data packet
    data_packet_t p;
    p.bits = r->bits;
    memcpy(p.data, r->buf, 4);

    // Send it to the queue
    xQueueSendToBack(queue, &p, 0);
}

static void task(void *arg)
{
    // Create queue
    queue = xQueueCreate(5, sizeof(data_packet_t));
    if (!queue)
    {
        ESP_LOGE(TAG_wiegand, "Error creating queue");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    // Initialize reader
    ESP_ERROR_CHECK(wiegand_reader_init(&reader, pinWGD0, pinWGD1,
                                        true, 4, reader_callback, WIEGAND_MSB_FIRST, WIEGAND_LSB_FIRST));

    data_packet_t p;
    while (1)
    {
        ESP_LOGI(TAG_wiegand, "Waiting for Wiegand data...");
        xQueueReceive(queue, &p, portMAX_DELAY);

        // dump received data
        printf("==========================================\n");
        printf("Bits received: %d\n", p.bits);
        printf("Received data:");
        int bytes = p.bits / 8;
        int tail = p.bits % 8;
        for (size_t i = 0; i < bytes + (tail ? 1 : 0); i++)
            printf(" 0x%02x", p.data[i]);
        printf("\n==========================================\n");
    }
}
#pragma endregion Wiegand

#pragma region GPIO
static void init_gpio(void)
{
    /*
    for (uint8_t i = 0; i <= obj_count_pin_gpio; i ++)
    {
        if (obj_ref[i][obj_col_pin] != 0 && obj_ref[i][obj_col_pin] <= 99) // GPIO pins
        {
            ESP_LOGI(TAG_gpio, "Pin: %i  Mode: %i  Pull: %i  Intr: %i", obj.pin[i], obj.pin_mode[i], obj.pin_pull[i], obj.pin_interupt[i]);
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = 1ULL<<obj.pin[i]; // GPIO_OUTPUT_PIN_SEL;

            if (obj.pin_mode == 1) io_conf.mode =  GPIO_MODE_INPUT;
            else io_conf.mode =  GPIO_MODE_OUTPUT;

            if (obj.pin_interupt == 1) io_conf.intr_type = GPIO_INTR_ANYEDGE;
            else io_conf.intr_type =  GPIO_INTR_DISABLE;

            if (obj.pin_pull == 1) 
            {
                io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            }

            else if (obj.pin_pull == 2) 
            {
                io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            }

            else 
            {
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            }
            
            gpio_config(&io_conf);
        }
    }
    */
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

 }

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    int val;

    for (;;) 
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
        {
            val = gpio_get_level(io_num);
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, val);
            ESP_LOGI(TAG_gpio, "GPIO[%"PRIu32"] intr, val: %d\n", io_num, val);
            parse_io(io_num, val);
        }
    }
}

void set_pin(uint8_t pin, bool state)
{
    if (state) gpio_set_level(pin, 0);
    else gpio_set_level(pin, 1);
}
#pragma endregion GPIO

#pragma region WiFi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } 

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if (s_retry_num < 3) 
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_wifi, "retry to connect to the AP");
        } 
        
        else 
        {
            xEventGroupSetBits(wifi_event_group, FAIL_BIT);
        }

        ESP_LOGI(TAG_wifi,"connect to the AP fail");
    } 

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_wifi, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        active_wifi = true;
        // sprintf(wifi_ip, IPSTR, IP2STR(&event->ip_info.ip));
        // wifi_ip = IP2STR(&event->ip_info.ip.addr);
    }
}

void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_LOGI(TAG_wifi, "** Debug 3");
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG_wifi, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & CONNECTED_BIT) 
    {
        ESP_LOGI(TAG_wifi, "connected to ap SSID:%s", WIFI_SSID);
    } 
    
    else if (bits & FAIL_BIT) 
    {
        ESP_LOGI(TAG_wifi, "Failed to connect to SSID:%s", WIFI_SSID);
    } 
    
    else 
    {
        ESP_LOGE(TAG_wifi, "UNEXPECTED EVENT");
    }
}
#pragma endregion WiFi