// General
#define enable_ota
#define enable_mqtt
// #define initconfig

// WiFi
#define WIFI_SSID "Gigaclear_417E"
#define WIFI_PASS "fxeyibjydr"

// Objects
#define obj_count               30
#define obj_count_state         17
#define obj_count_pin_gpio      24
#define obj_count_pin_mcp23x17  16
#define obj_count_power         9
#define obj_count_temp          5
#define obj_col_state           0
#define obj_col_pin             1
#define obj_col_power           2
#define obj_col_temp            3

#define obj_mount	            0
#define obj_dslr	            1
#define obj_focuser	            2
#define obj_heaters	            3
#define obj_ancillary	        4
#define obj_ctrl1	            5
#define obj_roof_lock	        6
#define obj_roof_motor	        7
#define obj_roof_motor_open	    8
#define obj_roof_motor_close    9
#define obj_dslr_cooler	        10
#define obj_laser	            11
#define obj_roof_motor_pwm	    12
#define obj_sen_roof_closed	    13
#define obj_sen_roof_open	    14
#define obj_sen_railing_n	    15
#define obj_sen_railing_e	    16
#define obj_sen_railing_s	    17
#define obj_sen_railing_w	    18
#define obj_sen_pier	        19
#define obj_sen_hatch	        20
#define obj_mount_park	        21
#define obj_sda	                22
#define obj_scl	                23
#define obj_onewire	            24
#define obj_ota_main	        25
#define obj_ota_guide	        26
#define obj_temp_air	        27
#define obj_humidity	        28
#define obj_heater_mode	        29
#define obj_heater_value	    30

// Timers
#define sensors_read_interval   2000
#define send_status_interval    2000

// MQTT
#define cmdSysSoftReset     "sr00"
#define cmdSysDebugOn       "d100"
#define cmdSysDebugOff      "d000"

#define ds18b20HexRegs      0x28, 0xFF, 0x8E, 0xC9, 0x61, 0x16, 0x04, 0xA2


