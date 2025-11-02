#pragma once
#include "config.h"

extern int16_t state_roof_pos;
extern bool enable_watchdog;
extern char *status_msg;
extern char *status_task;
extern const uint8_t obj_ref[obj_count][4];

extern struct Obj {
    float temp[obj_count_temp];
    float v[obj_count_power];
    float v_shunt[obj_count_power];
    float i[obj_count_power];
    float w[obj_count_power];
    float w_norm[obj_count_power];
    uint8_t state[obj_count];
    const uint8_t pin[obj_count_pin_gpio + obj_count_pin_mcp23x17]; /* 0-39 GPIO, 40-55 MCP23x17 */
    uint8_t pin_mode[obj_count_pin_gpio + obj_count_pin_mcp23x17];  /* 1 = input, 2 = output */
    uint8_t pin_pull[obj_count_pin_gpio];                           /* 0 = disabled, 1 = pullup, 2 = pulldown */
    uint8_t pin_interupt[obj_count_pin_gpio]                        /* 0 = disabled, 1 = rising, 2 = falling, 3 = rising & falling, 4 = input low level trigger, 5 = input high level trigger */
};
extern struct Obj obj;

void parse_io(uint32_t io_num, int val);
void parse_mqtt(char *msg, int msgLen);
void pinDigitalSet(uint8_t obj, bool state, bool is_relay);
