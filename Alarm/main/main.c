#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <esp_timer.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "hardware.c"
#include "mqtt.c"
#include "nvs.c"
#include "ota.c"
#include "program.h"
#include "config.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

static void config_read(void);
static void init_config(void);
void send_status(void *pvParameters);

static const char *ver = "1.0.1";
static const char *TAG_main = "main";

void app_main(void)
{ 
    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG_main, "System Startup: Ver %s", ver);

    ESP_LOGI(TAG_main, "Show hardware");
    showESPHardware();

    ESP_LOGI(TAG_main, "Init NVS");
    init_nvs();
    
    #ifdef initconfig
    ESP_LOGI(TAG_main, "Set initial config");
    init_config();
    #endif

    ESP_LOGI(TAG_main, "Read config from NVS");
    config_read();

    ESP_LOGI(TAG_main, "Init WiFi");
    wifi_init_sta();
    if (active_wifi)
    {
        #ifdef enable_mqtt
        ESP_LOGI(TAG_main, "Init MQTT");
        init_mqtt();
        mqttPub(status_msg);
        mqttSub(mqttChanIn);
        #endif

        #ifdef enable_ota
        ESP_LOGI(TAG_main, "Init Webserver & OTA");
        init_http();
        #endif
    }

    ESP_LOGI(TAG_main, "Init GPIO");
    init_gpio();

    ESP_LOGI(TAG_main, "Init sensors read task");
    xTaskCreate(sensors_read, "sensors_readr", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG_main, "Init send status data task");
    xTaskCreate(send_status, "send_status", 4096, NULL, 5, NULL);

    status_msg = "Running";
    ESP_LOGI(TAG_main, "End of main");
}

static void config_read(void)
{
    wifi_ssid = nvs_getStr("wifi_ssid");
    wifi_pass = nvs_getStr("wifi_pass");
    ota_user = nvs_getStr("http_user");
    ota_pass = nvs_getStr("http_pass");

    obj.w_norm[0] = strtof(nvs_getStr("0"), NULL);
    obj.w_norm[1] = strtof(nvs_getStr("1"), NULL);
    obj.w_norm[2] = strtof(nvs_getStr("2"), NULL);
    obj.w_norm[3] = strtof(nvs_getStr("3"), NULL);
    obj.w_norm[4] = strtof(nvs_getStr("4"), NULL);
    obj.w_norm[5] = strtof(nvs_getStr("5"), NULL);
    obj.w_norm[6] = strtof(nvs_getStr("6"), NULL);
    obj.w_norm[7] = strtof(nvs_getStr("7"), NULL);
    obj.w_norm[8] = strtof(nvs_getStr("8"), NULL);
}

static void init_config(void)
{
    nvs_setStr("wifi_ssid", "Gigaclear_417E");
    nvs_setStr("wifi_pass", "fxeyibjydr");
    nvs_setStr("mqtt_live", "mqtt://192.168.1.87:1883");
    nvs_setStr("mqttChanIn", "ocsCmd");
    nvs_setStr("mqttChanOut", "ocsStatus");
    nvs_setStr("http_user", "admin");
    nvs_setStr("http_pass", "esp32$");
    nvs_setStr("0", "1.1");
    nvs_setStr("1", "2.2");
    nvs_setStr("2", "3.3");
    nvs_setStr("3", "4.4");
    nvs_setStr("4", "5.5");
    nvs_setStr("5", "6.6");
    nvs_setStr("6", "2.2");
    nvs_setStr("7", "3.3");
    nvs_setStr("8", "4.4");
    nvs_setStr("9", "5.5");
    nvs_setStr("10", "6.6");
    
}

void send_status(void *pvParameters)
{
    while (1)
    {
        char msg[400] = { ">1" };
        char state[4];
        char splits_power[50];
        char splits_sensors[180];
        char splits_status[120];

        for (int i = 0; i <= obj_count_state; i ++)
        {
            sprintf(state, "%i", obj.state[i]);
            strcat(msg, state); //obj.state[i]);
        }

        for (int i = 0; i <= obj_count_power; i ++)
        {
            sprintf(splits_power, ",%f,%f", obj.v[i], obj.i[i]);
            strcat(msg, splits_power);
        }

        for (int i = 0; i <= obj_count_temp; i ++)
        {
            sprintf(splits_sensors, ",%f", obj.temp[i]);
            strcat(msg, splits_sensors);
        }

        sprintf(splits_status, ",%lld,%s,%s<", esp_timer_get_time() / 1000000, status_task, status_msg);
        strcat(msg, splits_status);

        ESP_LOGV(TAG_main, "Sending status - %s", msg);
        mqttPub(msg);
        vTaskDelay(send_status_interval / portTICK_PERIOD_MS);
    }
}


