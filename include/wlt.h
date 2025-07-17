#ifndef WLT_H
#define WLT_H

#include "pico/cyw43_arch.h"
#include "wlt_tcp.h"
#include "general.h"
#include "eeprom_24LC256.h"

#define WIFI_SSID_MAX_LEN                   32
#define WIFI_PASS_MAX_LEN                   64
#define WIFI_PASS_MIN_LEN                   8
#define WIFI_PASS_HIDDEN                    ""
#define WIFI_PASS_VALID                     1
#define WIFI_PASS_INVALID                   0
#define WIFI_PASS_NOT_CHANGE                -1

#define WIFI_DEVICENAME_MAX_LEN             32

#define WIFI_AP_SSID                        "PICOW-WIFI"
#define WIFI_AP_PASS                        "PicoWifiPass"

#define WIFI_DEFAULT_DEVICENAME             "WiFi Sensor"

#define GPIO_SELECT_WIFI_MODE               22

typedef enum wlt_wifi_mode {
    WLT_WIFI_MODE_STA = 0,
    WLT_WIFI_MODE_AP = 1
} wlt_wifi_mode_t;

typedef enum wlt_error{
    WLT_SUCCESS = 0,
    WLT_GENERIC_ERROR = -1,
    WLT_INVALID_ARGUMENT = -2
} wlt_error_t;

typedef struct wlt_net_config {
    uint8_t devicename[WIFI_DEVICENAME_MAX_LEN];
    uint8_t wifi_mode;
    uint8_t wifi_option; // reserved for future use
    uint8_t reserved[2]; // reserved for future use
    uint8_t wifi_ssid[WIFI_SSID_MAX_LEN];
    uint8_t wifi_pass[WIFI_PASS_MAX_LEN];
    u32_t ipaddr;
    u32_t ipmask;
    u32_t gwaddr;
} wlt_net_config_t;

#define T_FORMAT_CELSIUS        0
#define T_FORMAT_FAHRENHEIT     1
#define OUT_FORMAT_CSV          0
#define OUT_FORMAT_TXT          1
#define SENS_NOT_AVAILABLE      0
#define SENS_AVAILABLE          1
#define SENS_DATA_NOT_VALID     0
#define SENS_DATA_VALID         1
#define POLL_READ_TIME_MIN      1   // Minimum poll read sensor time in seconds (1 second)
#define POLL_READ_TIME_MAX      63  // Maximum poll read sensor time in seconds (6 bits)
#define POLL_READ_TIME_DFLT     30  // Default poll read sensor time in seconds

typedef union settings {
    uint16_t all_options;
    struct {
        uint8_t t_format    : 1; // Bit 0 - Temperature format 0 = Celsius, 1 = Fahrenheit
        uint8_t out_format  : 1; // Bit 1 - Output format 0 = CSV, 1 = TXT
        uint8_t poll_time   : 6; // Bit 2-7 - Poll time in seconds (0-63 seconds)
        
        uint8_t trd_hyst    : 3; // Bit 8-10 - Number of consecutive read to trigger thresholds
        uint8_t sens_avail  : 1; // Bit 11 - Sensor availability: 0 = not available, 1 = available
        uint8_t data_valid  : 1; // Bit 12 - tell me if data read from sensor is valid: 0 = not valid, 1 = valid
        uint8_t reserved    : 3; // Bit 13-15 - Reserved for future use
    } options;
} settings_t;

enum trd_trigger {
    TRD_TRIGGER_NONE    = 0,     // No trigger
    TRD_TRIGGER_HIGH    = 1,     // Trigger when value is up the threshold
    TRD_TRIGGER_LOW     = 2,      // Trigger when value is below the threshold
    TRD_TRIGGER_BOTH    = 3      // Trigger when value is up and below the threshold
};

typedef struct trd {
    float   value;      // threshold value
    uint8_t trigger;    // type of trigger
} trd_t;

typedef struct threshold {
    trd_t temperature; // Temperature threshold
    trd_t humidity;    // Humidity threshold
    trd_t pressure;    // Pressure threshold
} threshold_t;

typedef struct thresholds {
    threshold_t high;  // High thresholds
    threshold_t low;   // Low thresholds
} thresholds_t;

typedef struct wlt_data {
    float           temperature;
    float           humidity;
    float           pressure; // for future use, not available with DHT20 sensor
    settings_t      settings;
    thresholds_t    thresholds;
} wlt_data_t;

typedef struct wlt_run_time_config {
    wlt_net_config_t net_config;
    wlt_data_t data;
} wlt_run_time_config_t;

typedef struct wlt_config_data {
    char            signature[EEPROM_CTRL_WORD_LEN];
    uint8_t         devicename[WIFI_DEVICENAME_MAX_LEN];
    uint8_t         wifi_ssid[WIFI_SSID_MAX_LEN];
    uint8_t         wifi_pass[WIFI_PASS_MAX_LEN];
    settings_t      settings;
    thresholds_t    thresholds;
} wlt_config_data_t;

typedef struct wlt_server {
    TCP_SERVER_T *state;
    ip_addr_t ip_addr;
    ip_addr_t ip_mask;
} wls_server_t;

#endif // WLT_H
