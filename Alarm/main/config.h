// General
#define enable_ota
#define enable_mqtt
// #define initconfig

// WiFi
#define WIFI_SSID "Gigaclear_417E"
#define WIFI_PASS "fxeyibjydr"

// Pins
#define pinWGD0                 21
#define pinWGD1                 22

// Timers
#define sensors_read_interval   2000
#define send_status_interval    2000

// MQTT
#define cmdSysSoftReset     "sr00"
#define cmdSysDebugOn       "d100"
#define cmdSysDebugOff      "d000"

// Wiegand
#define almCodeArm          "11"
#define almCodeDisarm       "1346"
#define wgCodeStart         27
#define wgCodeEnd           13

#define ds18b20HexRegs      0x28, 0xFF, 0x8E, 0xC9, 0x61, 0x16, 0x04, 0xA2


