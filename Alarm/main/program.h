#pragma once
#include "config.h"

extern uint8_t doorCodeBit;
extern int16_t state_roof_pos;
extern bool enable_watchdog;
extern char *status_msg;
extern char *status_task;

void parse_wiegand(uint8_t code[4]);
void checkDoorCode();
void parse_io(uint32_t io_num, int val);
void parse_mqtt(char *msg, int msgLen);
void pinDigitalSet(uint8_t obj, bool state, bool is_relay);
