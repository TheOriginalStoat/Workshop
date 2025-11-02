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

int16_t state_roof_pos;
bool enable_watchdog = true;
char *status_msg = ">1,0,Startup,<";
char *status_task = "none";
static const char *TAG_program = "program";

void set_pin(uint8_t pin, bool state);
void set_mcp_pin(uint8_t pin, bool state);

const uint8_t pin[obj_count_pin_gpio + obj_count_pin_mcp23x17] = { 0, 2, 4, 5, 12, 13, 14, 15, 
    16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39, 
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55 };   /* 0-39 GPIO, 40-55 MCP23x17 */

const uint8_t obj_ref[obj_count][4] = { 
{	0	,	45	,	0	,	0	}	,	//	obj_mount
{	1	,	46	,	1	,	0	}	,	//	obj_dslr
{	2	,	0	,	2	,	0	}	,	//	obj_focuser
{	3	,	43	,	3	,	0	}	,	//	obj_heaters
{	4	,	44	,	4	,	0	}	,	//	obj_ancillary
{	0	,	0	,	5	,	0	}	,	//	obj_ctrl1
{	0	,	47	,	6	,	0	}	,	//	obj_roof_lock
{	0	,	0	,	7	,	0	}	,	//	obj_roof_motor
{	5	,	49	,	0	,	0	}	,	//	obj_roof_motor_open
{	6	,	50	,	0	,	0	}	,	//	obj_roof_motor_close
{	7	,	0	,	8	,	0	}	,	//	obj_dslr_cooler
{	8	,	0	,	0	,	0	}	,	//	obj_laser
{	0	,	15	,	0	,	0	}	,	//	obj_roof_motor_pwm
{	9	,	52	,	0	,	0	}	,	//	obj_sen_roof_closed
{	10	,	51	,	0	,	0	}	,	//	obj_sen_roof_open
{	11	,	53	,	0	,	0	}	,	//	obj_sen_railing_n
{	12	,	0	,	0	,	0	}	,	//	obj_sen_railing_e
{	13	,	0	,	0	,	0	}	,	//	obj_sen_railing_s
{	14	,	0	,	0	,	0	}	,	//	obj_sen_railing_w
{	15	,	54	,	0	,	0	}	,	//	obj_sen_pier
{	16	,	55	,	0	,	0	}	,	//	obj_sen_hatch
{	0	,	0	,	0	,	0	}	,	//	obj_mount_park
{	0	,	21	,	0	,	0	}	,	//	obj_sda
{	0	,	22	,	0	,	0	}	,	//	obj_scl
{	0	,	0	,	0	,	0	}	,	//	obj_onewire
{	0	,	0	,	0	,	0	}	,	//	obj_ota_main
{	0	,	0	,	0	,	1	}	,	//	obj_ota_guide
{	0	,	0	,	0	,	2	}	,	//	obj_temp_air
{	0	,	0	,	0	,	3	}	,	//	obj_humidity
{	17	,	0	,	0	,	0	}	,	//	obj_heater_mode
{	0	,	0	,	0	,	4	}		//	obj_heater_value
};

struct Obj obj;

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
}

