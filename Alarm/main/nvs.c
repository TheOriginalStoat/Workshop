#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

nvs_handle_t my_handle;
esp_err_t err;
static const char *TAG_nvs = "nvs";

typedef struct 
{
    nvs_type_t type;
    const char *str;
} type_str_pair_t;

// static const size_t TYPE_STR_PAIR_SIZE = sizeof(type_str_pair) / sizeof(type_str_pair[0]);

void init_nvs(void)
{
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGI(TAG_nvs, "NVS flash_erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    ESP_LOGI(TAG_nvs, "Opening Non-Volatile Storage (NVS) handle...");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_nvs, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
}

void nvs_seti32(char *name, int32_t val)
{
    ESP_LOGI(TAG_nvs, "Writing %ld to %s", val, name);
    err = nvs_set_i32(my_handle, name, val);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG_nvs, "Failed to write counter!");
    }
    // else fred();
}

int32_t nvs_geti32(char *name)
{
    ESP_LOGI(TAG_nvs, "Reading %s", name);
    int32_t val = -99;
    err = nvs_get_i32(my_handle, name, &val);
    switch (err) 
    {
        case ESP_OK:
            return val;
            break;

        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG_nvs, "The value is not initialized yet!");
            break;

        default:
            ESP_LOGE(TAG_nvs, "Error (%s) reading!", esp_err_to_name(err));
    }
    return -99;
}

void nvs_setStr(char *name, char *val)
{
    ESP_LOGI(TAG_nvs, "Writing %s to %s", val, name);
    err = nvs_set_str(my_handle, name, val);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG_nvs, "Failed to write string!");
    }
    // else fred();
}

char* nvs_getStr(char *name)
{
    size_t required_size = 0;
    ESP_LOGV(TAG_nvs, "Reading %s", name);
    err = nvs_get_str(my_handle, name, NULL, &required_size);
    if (err == ESP_OK) 
    {
        char* val = malloc(required_size);
        err = nvs_get_str(my_handle, name, val, &required_size);
        if (err == ESP_OK) 
        {
            ESP_LOGV(TAG_nvs, "Read string: %s", val);
            return val;
        }
        free(val);
    }
    return NULL;
}

void nvs_close_handle()
{
    ESP_LOGI(TAG_nvs, "Closing storage handle");
    nvs_close(my_handle);
}
