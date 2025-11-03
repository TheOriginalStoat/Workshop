#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <esp_timer.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "program.h"
#include "config.h"

uint8_t doorCodeBit = 0;
int16_t state_roof_pos;
bool enable_watchdog = true;
char *status_msg = ">1,0,Startup,<";
char *status_task = "none";
char* doorCodeEntered;
static const char *TAG_program = "program";

void set_pin(uint8_t pin, bool state);
void set_mcp_pin(uint8_t pin, bool state);

void parse_wiegand(uint8_t code[4])
{
    if (code == wgCodeStart) doorCodeBit = 1;

    else if (doorCodeBit >= 1 && doorCodeBit <= 4) 
    {
      doorCodeEntered[doorCodeBit] = code;
      doorCodeBit =+ 1;
    }

    else if (code == wgCodeEnd) 
    {
      checkDoorCode();
      doorCodeBit = 0;
    }
}

void checkDoorCode()
{
  if (strcmp(doorCodeEntered, almCodeDisarm) == 0)
  {
    ESP_LOGI(TAG_program, "Alarm Disarmed");
    // digitalWrite(pinLEDArmed, 0); 
    // digitalWrite(pinLEDDisarmed, 1);   
    // pinDigital("Relay Siren", pinRelaySiren, relayOff);
  }

  else if (strcmp(doorCodeEntered, almCodeArm) == 0)
  {
    ESP_LOGI(TAG_program, "Alarm Armed");
    // digitalWrite(pinLEDDisarmed, 0);
    // digitalWrite(pinLEDArmed, 1);  
  }

  else ESP_LOGI(TAG_program, "Invalid Code");
}

void parse_io(uint32_t io_num, int val)
{
    switch (io_num) // +100 for MCP23x17 pins
    {
        case 2: // Test
        if (val == 1) printf("Roof closed");
        break;

        case 100: // Mount
        ESP_LOGV(TAG_program, "** Debug - Mount cmd");
        break;

        case 101: // DSLR
        ESP_LOGV(TAG_program, "** Debug - DSLR cmd");
        break;
    }
}

void parse_mqtt(char *msg_in, int msgLen)
{
    if (msgLen == 11) // Normal command
    {
        char msg[12] = {"abcdefghijk\0"}; 
        const char start_id[] = ">1";
        char start[4];
        ESP_LOGV(TAG_program, "** Debug - good len %i", strlen(msg));

        for (uint8_t i = 0; i <= 11; i ++)
        {
            msg[i] = msg_in[i];
        }

        sprintf(start, "%.*s", 2, msg);

        if (strncmp(start, start_id, sizeof(start)) == 0)
        {
            char cmd[5];
            char val[5];
            strncpy(cmd, msg + 2, 4);
            cmd[4] = '\0';
            strncpy(val, msg + 6, 4);
            val[4] = '\0';

            ESP_LOGI(TAG_program, "** Debug - cmd: %s  val: %s", cmd, val);
            
           if (strcmp(cmd, cmdSysSoftReset) == 0)
            {
                ESP_LOGI(TAG_program, "Software reset recived");
                esp_restart();
            }
        }
    }
}

void pinDigitalSet(uint8_t obj_source, bool state, bool is_relay)
{
    /*
    uint8_t pin = obj.pin[obj_ref[obj_source][obj_col_pin]];
    ESP_LOGI(TAG_program, "PinSet - obj_source:%i  pin:%i  state:%i  relay?:%i", obj_source, pin, state, is_relay);

    if (pin <= obj_count_pin_gpio)
    {
        if (is_relay) set_pin(obj.pin[obj_ref[obj_source][obj_col_pin]], !state);
        else set_pin(obj.pin[obj_ref[obj_source][obj_col_pin]], state);
    }

    else
    {
        if (is_relay) set_mcp_pin(obj.pin[obj_ref[obj_source][obj_col_pin]], !state);
        else set_mcp_pin(obj.pin[obj_ref[obj_source][obj_col_pin]], state);
    }
        */
}

