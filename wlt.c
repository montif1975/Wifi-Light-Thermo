#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hardware/gpio.h"
//#include <ctype.h>
#include "hardware/i2c.h"
//#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
//#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "include/wlt.h"
#include "include/dht20.h"
#include "include/uart.h"
#include "include/eeprom_24LC256.h"
#include "include/rgb.h"

// global variables
wlt_run_time_config_t *prtconfig;
wlt_config_data_t *pconfig;

/*
 * Function: wlt_send_to_uart()
 * Send to UART the temperature and humidity in the requested format.
 */
void wlt_send_to_uart(float temperature, float humidity, uint8_t temp_format, uint8_t out_format)
{
    char app_string[24];
    float temp;

    memset(app_string,0,sizeof(app_string));
    if(temp_format == T_FORMAT_FAHRENHEIT)
        temp = C2F(temperature);
    else
        temp = temperature;

    switch(out_format)
    {
        case OUT_FORMAT_TXT:
            // add \r\n in order to be sure to print one measure on each line 
            // in the Windows/Unix terminal (without change its configuration)
            if(temp_format == T_FORMAT_FAHRENHEIT)
                sprintf(app_string,"%.02f °F - %.02f %%RH\r\n",temp,humidity);
            else
                sprintf(app_string,"%.02f °C - %.02f %%RH\r\n",temp,humidity);
            break;

        case OUT_FORMAT_CSV:
            // use ";" as CSV separator
            sprintf(app_string,"%.02f;%.02f\r\n",temp,humidity);
            break;
        
        default:
            break;
    }
    // send the string to UART
    uart_puts(UART_ID, app_string);

    return;
}

/*
 * Function: wlt_init_uart() 
*/
void wlt_init_uart(void)
{
    int baudrate;
    // Set up our UART with a basic baud rate.
    uart_init(UART_ID, 2400);

    // Set the TX and RX pins by using the function select on the GPIO
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed
    // The call will return the actual baud rate selected, which will be as close as
    // possible to that requested
    baudrate = uart_set_baudrate(UART_ID, BAUD_RATE);
    PRINT_DEBUG("Set up UART at baudrate %d\n", baudrate);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);
    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // for the moment we don't use the UART in RX mode

    // Now enable the UART to send interrupts - RX only
//    uart_set_irq_enables(UART_ID, true, false);

    // initialization UART completed, send a welcome message
    uart_puts(UART_ID, "\r\nlightThermo ready to work...\r\n");
    uart_puts(UART_ID, "Read temperature and humidity every minutes\r\n");

    return;
}

/*
 * Function: wlt_init_port_sensor()
*/
void wlt_init_port_sensor(void)
{
    // I2C for Sensor (Temp, humidity,etc...)
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT_SENS, 400*1000);
    
    gpio_set_function(I2C_SDA_SENS, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENS, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENS);
    gpio_pull_up(I2C_SCL_SENS);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    
    return;
}


/*
 * High level EEPROM functions
*/ 

/*
 * Function: wlt_read_config()
 * Description: This function reads the configuration from the EEPROM.
 * It returns EE_SUCCESS if the configuration is read correctly, otherwise it returns EE_ERROR. 
*/
int wlt_read_config(BYTE *config, int len)
{
    int ret;

    PRINT_DEBUG_N("Read config from EEPROM\n");

    ret = i2c_eeprom_read(EEPROM_START_ADDR, config, len);

#if DEBUG
    int i;

    // print the content of memory read
    for(i=0; i<len; i++)
    {
        printf("0x%02X ",config[i]);
        if((i + 1) % 8 == 0)
            printf("\n");
    }
#endif

    return ret;
}

/*
 * Function: wlt_write_config()
 * Description: This function writes the configuration to the EEPROM.
 * It returns EE_SUCCESS if the configuration is written correctly, otherwise it returns EE_ERROR.
*/
int wlt_write_config(BYTE *config, int len)
{
    int ret = EE_SUCCESS;

    PRINT_DEBUG_N("Write config to EEPROM\n");

    ret = i2c_eeprom_write(EEPROM_START_ADDR, config, len);

    return ret;
}

/*
 * Function: check_config()
 * Description: This function checks the configuration read from the EEPROM.
 * It returns EE_SUCCESS if the configuration is valid, otherwise it returns EE_ERROR.
 * The configuration is valid if the control word matches the expected value.
*/
int wlt_check_config(BYTE *signature, int len)
{
    if (signature == NULL || len < EEPROM_CTRL_WORD_LEN) {
        return EE_ERROR;
    }

    // Check the control word
    if (memcmp(signature, EEPROM_CTRL_WORD, EEPROM_CTRL_WORD_LEN) != 0) {
        return EE_ERROR;
    }

    return EE_SUCCESS;
}

/**
 * Function: wlt_update_config()
 * Description: This function updates the configuration (saved in the EEPROM)
 * with the provided runtime configuration data.
 */
void wlt_update_config(wlt_run_time_config_t *rt_config, wlt_config_data_t *config)
{
    if (rt_config == NULL || config == NULL) {
        printf("Invalid argument: rt_config or config is NULL\n");
        return;
    }
    strncpy(config->devicename,rt_config->net_config.devicename, sizeof(config->devicename) - 1);
    config->devicename[sizeof(config->devicename) - 1] = '\0'; // Ensure null termination
    strncpy(config->wifi_ssid, rt_config->net_config.wifi_ssid, sizeof(config->wifi_ssid) - 1);
    config->wifi_ssid[sizeof(config->wifi_ssid) - 1] = '\0'; // Ensure null termination
    strncpy(config->wifi_pass, rt_config->net_config.wifi_pass, sizeof(config->wifi_pass) - 1);
    config->wifi_pass[sizeof(config->wifi_pass) - 1] = '\0'; // Ensure null termination
    config->settings.all_options = rt_config->data.settings.all_options;
    config->thresholds = rt_config->data.thresholds;
    // Copy the signature (includes \0 at the end)
    strncpy(config->signature, EEPROM_CTRL_WORD, EEPROM_CTRL_WORD_LEN);
 
    return;
}

/*
 * Function: wlt_load_config()
 * Description: This function loads the configuration from the EEPROM into the runtime configuration.
 * It does not return any value.
 */
void wlt_load_config(wlt_run_time_config_t *rt_config, wlt_config_data_t *config)
{
    if (rt_config == NULL || config == NULL) {
        printf("Invalid argument: rt_config or config is NULL\n");
        return;
    }
    // Copy the device name
    strncpy(rt_config->net_config.devicename, config->devicename, sizeof(rt_config->net_config.devicename) - 1);
    rt_config->net_config.devicename[sizeof(rt_config->net_config.devicename) - 1] = '\0'; // Ensure null termination
    // Copy the WiFi SSID and password
    strncpy(rt_config->net_config.wifi_ssid, config->wifi_ssid, sizeof(rt_config->net_config.wifi_ssid) - 1);
    rt_config->net_config.wifi_ssid[sizeof(rt_config->net_config.wifi_ssid) - 1] = '\0'; // Ensure null termination
    strncpy(rt_config->net_config.wifi_pass, config->wifi_pass, sizeof(rt_config->net_config.wifi_pass) - 1);
    rt_config->net_config.wifi_pass[sizeof(rt_config->net_config.wifi_pass) - 1] = '\0'; // Ensure null termination
    rt_config->data.settings.all_options = config->settings.all_options;
    rt_config->data.thresholds = config->thresholds; 

    return;
}

/**
 * Function: wlt_update_and_save_config()
 * Description: This function updates the runtime configuration with the provided configuration data
 * and saves it to the EEPROM.
 * It does not return any value.
 */
void wlt_update_and_save_config(wlt_run_time_config_t *rt_config, wlt_config_data_t *config)
{
    wlt_update_config(prtconfig, pconfig);
    // I don't want to save some runtime data located in settings field (TODO change their position)
    pconfig->settings.options.sens_avail = SENS_NOT_AVAILABLE;
    pconfig->settings.options.data_valid = SENS_DATA_NOT_VALID;
    // Save the configuration to EEPROM
    if (wlt_write_config((BYTE *)pconfig, sizeof(wlt_config_data_t)) != EE_SUCCESS) {
        printf("*** ERROR ****\nUnable to write EEPROM (I2C) Memory\n");
    } else {
        printf("Configuration written to EEPROM\n");
    }
    return;
}         

/*
* Function: wlt_init_run_time_config()
* Description: This function initializes the runtime configuration with default values. 
*/
void wlt_init_run_time_config(wlt_run_time_config_t *config)
{
    if (config == NULL) {
        printf("Invalid argument: config is NULL\n");
        return;
    }
    // initialize the configuration structure
    memset(config, 0, sizeof(wlt_run_time_config_t));
    strncpy((char *)config->net_config.devicename, WIFI_DEFAULT_DEVICENAME, sizeof(config->net_config.devicename) - 1);
    config->net_config.devicename[sizeof(config->net_config.devicename) - 1] = '\0'; // Ensure null termination

    config->net_config.wifi_mode = WLT_WIFI_MODE_STA;
    config->data.settings.options.t_format = T_FORMAT_CELSIUS; // Default temperature format is Celsius
    config->data.settings.options.out_format = OUT_FORMAT_CSV; // Default output format is CSV
    config->data.settings.options.poll_time = POLL_READ_TIME_DFLT; // Default poll time is 5 seconds
    config->data.settings.options.trd_hyst = 3; // Default threshold hysteresis is 3 consecutive reads
    config->data.settings.options.sens_avail = SENS_NOT_AVAILABLE; // Sensor not available by default
    config->data.settings.options.data_valid = SENS_DATA_NOT_VALID; // Data not valid by default
    config->data.temperature = 0.0f; // Initialize temperature
    config->data.humidity = 0.0f; // Initialize humidity
    config->data.pressure = 0.0f; // Initialize pressure

    config->data.thresholds.high.temperature.value = 30.0f; // Default high temperature threshold
    config->data.thresholds.high.temperature.trigger = TRD_TRIGGER_NONE; // Trigger when temperature is above the threshold
    config->data.thresholds.low.temperature.value = 15.0f; // Default low temperature threshold
    config->data.thresholds.low.temperature.trigger = TRD_TRIGGER_NONE; // Trigger when temperature is below the threshold

    config->data.thresholds.high.humidity.value = 70.0f; // Default high humidity threshold
    config->data.thresholds.high.humidity.trigger = TRD_TRIGGER_NONE; // Trigger when humidity is above the threshold
    config->data.thresholds.low.humidity.value = 30.0f; // Default low humidity threshold
    config->data.thresholds.low.humidity.trigger = TRD_TRIGGER_NONE; // Trigger when humidity is below the threshold

    config->data.thresholds.high.pressure.value = 1020.0f; // Default high pressure threshold
    config->data.thresholds.high.pressure.trigger = TRD_TRIGGER_NONE; // Trigger when pressure is above the threshold
    config->data.thresholds.low.pressure.value = 980.0f; // Default low pressure threshold
    config->data.thresholds.low.pressure.trigger = TRD_TRIGGER_NONE; // Trigger when pressure is below the threshold

    return;
}

/*
* Function: wlt_configure_gpio_select_wifi_mode()
* Description: This function configures the GPIO to select the wifi mode.
 */
void wlt_configure_gpio_select_wifi_mode()
{
    gpio_init(GPIO_SELECT_WIFI_MODE);
    gpio_set_dir(GPIO_SELECT_WIFI_MODE, GPIO_IN);
    gpio_pull_up(GPIO_SELECT_WIFI_MODE);

    return;
}

/*
* Function: wlt_check_wifi_mode()
* Description: This function checks the GPIO to select the wifi mode.
* Parameters: config - pointer to the runtime configuration.
*/
wlt_error_t wlt_check_wifi_mode(wlt_run_time_config_t *config)
{
    uint8_t count, sta_mode, ap_mode, max_loop;
    uint16_t delay;
    
    if (config == NULL) {
        return WLT_INVALID_ARGUMENT;
    }

    max_loop = 3;
    delay = 300;
    count = 0;
    sta_mode = 0;
    ap_mode =0;
    while (count < max_loop) {
        if (gpio_get(GPIO_SELECT_WIFI_MODE) == 0) {
            printf("Read GPIO (%d) value = 0\n", GPIO_SELECT_WIFI_MODE);
            ap_mode++;
        } else {
            printf("Read GPIO (%d) value != 0\n", GPIO_SELECT_WIFI_MODE);
            sta_mode++;
        }
        count++;
        sleep_ms(delay);
    }

    if (ap_mode == max_loop) {
        config->net_config.wifi_mode = WLT_WIFI_MODE_AP;
    } else 
        if (sta_mode == max_loop) {
        config->net_config.wifi_mode = WLT_WIFI_MODE_STA;
    } else {
        return WLT_GENERIC_ERROR;
    }

    return WLT_SUCCESS;
}

/*
 * Function: wlt_set_led_color()
 * Description: This function sets the LED color based on the index provided.
*/
void wlt_set_led_color(int index, wlt_rgb_led_t *led_state)
{
    int color;

    if(led_state == NULL) {
        // when the function is called with NULL, it means that the caller is error handling
        // in this case, we don't need to update the led_state but only set the color
        color = index & ~RGB_BLINK_OPT; // Remove the blink option bit
        rgb_set_led_color(index);
        return;
    }

    if((index & RGB_BLINK_OPT) == RGB_BLINK_OPT)
    {
        // If the index has the bit RGB_BLINK_OPT set, treat it as a blink option
        color = index & ~RGB_BLINK_OPT; // Remove the blink option bit
        led_state->blink = true; // Set the blink option
    } else {
        color = index;
    }
    if(color < 0 || color >= RGB_COLOR_MAX) {
        printf("Invalid RGB color index: %d\n", color);
        return;
    }
    led_state->color = color; // Set the current color
#if DEBUG
    printf("Setting LED color, index: %d\n", color);
#endif
    if(color != RGB_COLOR_OFF)
        led_state->is_on = true; // Set the LED state to on
    else
        led_state->is_on = false; // Set the LED state to off
    rgb_set_led_color(color);
    led_state->last_change = time_us_64(); // Update the last change time

    return;
}

/*
* Function: wlt_update_rgb_led()
 * Description: This function updates the LED color based on the current mode and the tick count.
 * It checks if the LED should be turned on or off based on the tick count and the start tick count saved in the led_status structure.
 * Parameters: config - pointer to the rgb led struct, tick - current tick count.
 */
void wlt_update_rgb_led(uint8_t mode, wlt_rgb_led_t *led_status, int64_t tick)
{
    int color = RGB_COLOR_OFF; // Default color is OFF
    int64_t delta;

    if (led_status == NULL) {
        printf("Invalid argument: config is NULL\n");
        return;
    }

    if (tick < led_status->last_change) {
        delta = (UINT64_MAX - led_status->last_change) + tick; // Handle wrap-around case
    } else {
        delta = tick - led_status->last_change; // Normal case
    }

    // retrieve the current color and blink option
    color = led_status->color;
    if(color & RGB_BLINK_OPT == 0x0) {
        // If the color has not the blink option, no need to update color
        return;
    }
    else {
        // check the led's status
        if(led_status->is_on == false) {
            // LED is off, check if the timer has expired
            if (delta < (RGB_LED_OFF_TIME_MS * 1000)) {
                // Timer has not expired, do not change the LED color
                return;
            } else {
                // Timer has expired, turn on the LED checking which color to set
                // according to the current mode (I can't use the color stored in led_status
                // because it is the OFF color when the LED is off)
                if(mode == WLT_WIFI_MODE_STA) {
                    color = RGB_LED_ON_STA_MODE;
                } else if (mode == WLT_WIFI_MODE_AP) {
                    color = RGB_LED_ON_AP_MODE;
                } else {
                    color = RGB_LED_ON_FAIL; // Default to red if mode is unknown
                }
                printf("Timeout led = %.02f [s] - Led was OFF, switch on to color %d\n", (float)(delta/1000000), (color & ~RGB_BLINK_OPT));
                wlt_set_led_color(color, led_status); // Turn on the LED
            }
        }
        else {
            // LED is on, check if the timer has expired
            if (delta < (RGB_LED_ON_TIME_MS * 1000)) {
                // Timer has not expired, do not change the LED color
                return;
            } else {
                // Timer has expired, turn off the LED
                printf("Timeout led = %.02f [s] - Led was ON with color %d, switch OFF\n", (float)(delta/1000000), led_status->color);
                wlt_set_led_color(RGB_COLOR_OFF, led_status); // Turn off the LED
            }
        }
    }

    return;
}

/*
* Function: wlt_goto_error()
 * Description: This function sets the RGB LED to red with blink option and enters an infinite loop to indicate an error state.
 * It does not return any value.
*/
void wlt_goto_error(int led_color)
{
    int color = led_color & ~RGB_BLINK_OPT; // Remove the blink option bit
    int do_blink = (led_color & RGB_BLINK_OPT) ? 1 : 0;

    if(do_blink) {
        while (1) {
            // Set the RGB LED to color
            wlt_set_led_color(color, NULL);
            sleep_ms(RGB_LED_ON_TIME_MS);
            wlt_set_led_color(RGB_COLOR_OFF, NULL);
            sleep_ms(RGB_LED_OFF_TIME_MS);
        }
    } else {
        // Just set the LED to the color without blinking
        wlt_set_led_color(color, NULL);
        while (1) {
            sleep_ms(1000);
        }
    }
    
    return;
}

/*
*   Main function
*/
int main()
{
    int ret;
    wlt_run_time_config_t run_time_config;
    wlt_config_data_t config;
    wlt_rgb_led_t rgb_led;
    dhcp_server_t dhcp_server;
    dns_server_t dns_server;
    wls_server_t wls_server;
    char led_on = 1;
    volatile int64_t tick, pre_tick;

    prtconfig = &run_time_config;
    pconfig = &config;
    srand(to_us_since_boot(get_absolute_time()));
    stdio_init_all();

    sleep_ms(2000);

    // hardware initialization
    i2c_eeprom_init();
    wlt_init_port_sensor();
    wlt_init_uart();
    rgb_init();

    // switch on the RGB LED
    wlt_set_led_color(RGB_LED_ON_BOOT,&rgb_led); // Set the LED color on boot

    // search config reading from memory
    ret = 0;
    // set default configuration
    wlt_init_run_time_config(prtconfig);
    memset(&config, 0x0, sizeof(config));
    if (wlt_read_config((BYTE *)&config, sizeof(config)) != EE_SUCCESS) {
        printf("*** ERROR ****\nUnable to read EEPROM (I2C) Memory\n");
        wlt_goto_error(RGB_LED_ON_FAIL);
    } else {
        printf("Config data CTRL WORD: %s\n", (char *)&config.signature);
        // check if the configuration read is valid
        if (wlt_check_config((char *)pconfig->signature, sizeof(config.signature)) != EE_SUCCESS) {
            printf("*** ERROR ****\nInvalid configuration read from EEPROM (I2C) Memory\n");
            wlt_goto_error(RGB_LED_ON_FAIL);
        } else {
            printf("Configuration read successfully from EEPROM\n");
            // configuration read successfully, initialize the runtime configuration loading data from EEPROM
            wlt_load_config(prtconfig, pconfig);
        }
    }
    if (ret != 0) {
        printf("Failed to read configuration, using default values\n");
        wlt_update_config(prtconfig, pconfig);
        // save the default configuration to EEPROM
        if (wlt_write_config((BYTE *)&config, sizeof(config)) != EE_SUCCESS) {
            printf("*** ERROR ****\nUnable to write EEPROM (I2C) Memory\n");
            wlt_goto_error(RGB_LED_ON_FAIL);
        } else {
            printf("Default configuration written to EEPROM\n");
        }
    }

    if (DHT20_init() != 0) {
        printf("Failed to initialize DHT20 sensor\n");
        prtconfig->data.settings.options.sens_avail = SENS_NOT_AVAILABLE;
    } else {
        printf("DHT20 sensor initialized successfully\n");
        prtconfig->data.settings.options.sens_avail = SENS_AVAILABLE;
    }

    // configure the GPIO to select the wifi mode.
    wlt_configure_gpio_select_wifi_mode();

    // check the GPIO to select the wifi mode.
    ret = wlt_check_wifi_mode(prtconfig);
    if (ret != WLT_SUCCESS) {
        printf("failed to check the wifi mode\n");
        wlt_goto_error(RGB_LED_ON_FAIL);
    } else {
        if(prtconfig->net_config.wifi_mode == WLT_WIFI_MODE_STA) {
            printf("Wi-Fi mode: STA\n");
            printf("Connecting to Wi-Fi SSID: %s\n", prtconfig->net_config.wifi_ssid);
            printf("Using Wi-Fi password: %s\n", prtconfig->net_config.wifi_pass);
        } else {
            printf("Wi-Fi mode: AP\n");
            // in AP mode we use hardcoded values for the SSID and password
            strncpy(prtconfig->net_config.wifi_ssid, WIFI_AP_SSID, sizeof(prtconfig->net_config.wifi_ssid));
            strncpy(prtconfig->net_config.wifi_pass, WIFI_AP_PASS, sizeof(prtconfig->net_config.wifi_pass));
        }
    }

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        wlt_goto_error(RGB_LED_ON_WIFI_FAIL);
    }
    // start switching on the LED        
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

    if(prtconfig->net_config.wifi_mode == WLT_WIFI_MODE_AP) {
        wlt_set_led_color(RGB_LED_ON_AP_MODE, &rgb_led); // Set the LED color for AP mode

        // Enable wifi access point
        cyw43_arch_enable_ap_mode(prtconfig->net_config.wifi_ssid, prtconfig->net_config.wifi_pass,CYW43_AUTH_WPA2_AES_PSK);

        wls_server.ip_addr.addr = PP_HTONL(CYW43_DEFAULT_IP_AP_ADDRESS);
        wls_server.ip_mask.addr = PP_HTONL(CYW43_DEFAULT_IP_MASK);
        printf("IP address (wls_server) 0x%X\n", wls_server.ip_addr.addr);

        // Start the dhcp server
        dhcp_server_init(&dhcp_server, &wls_server.ip_addr, &wls_server.ip_mask);
#if START_DNS_SERVER
        // Start the dns server
        dns_server_init(&dns_server, &wls_server.ip_addr);
#endif // START_DNS_SERVER
    } else {
        wlt_set_led_color(RGB_LED_ON_STA_MODE, &rgb_led); // Set the LED color for STA mode

        // Enable wifi station
        cyw43_arch_enable_sta_mode();

        printf("Connecting to Wi-Fi...\n");
        if (cyw43_arch_wifi_connect_timeout_ms(prtconfig->net_config.wifi_ssid, prtconfig->net_config.wifi_pass, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("failed to connect.\n");
            // TODO: Add a retry mechanism
            // or swith to AP mode; blink the LED to indicate the error?
            wlt_goto_error(RGB_LED_ON_WIFI_FAIL);
        } else {
            printf("Connected.\n");
            prtconfig->net_config.ipaddr = cyw43_state.netif[0].ip_addr.addr;
            prtconfig->net_config.ipmask = cyw43_state.netif[0].netmask.addr; 
            prtconfig->net_config.gwaddr = cyw43_state.netif[0].gw.addr;
            // Set the wls_server ip_addr and ip_mask
            wls_server.ip_addr.addr = cyw43_state.netif[0].ip_addr.addr;
            wls_server.ip_mask.addr = cyw43_state.netif[0].netmask.addr;
#if DEBUG
            printf("IP address (cyw43) )0x%X\n", cyw43_state.netif[0].ip_addr.addr);
            printf("IP address (wls_server) 0x%X\n", wls_server.ip_addr.addr);
#endif // DEBUG
            // Read the ip address in a human readable way
            printf("IP address %s\n",ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipaddr)));
            printf("IP mask %s\n", ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipmask)));
            printf("Gateway %s\n", ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.gwaddr)));
        }
    }

    // Start the tcp server
    wls_server.state = (TCP_SERVER_T *)calloc(1, sizeof(TCP_SERVER_T));
    if (!wls_server.state) {
        printf("Fatal error: failed to allocate wls_server.state\n");
        wlt_goto_error(RGB_LED_ON_FAIL);
    }
    wls_server.state->server_pcb = NULL;
    wls_server.state->complete = false;
    wls_server.state->gw.addr = wls_server.ip_addr.addr;

    if (!tcp_server_open(wls_server.state, prtconfig->net_config.wifi_ssid)) {
        printf("Failed to open server\n");
        wlt_goto_error(RGB_LED_ON_FAIL);
    }
    printf("Server opened successfully\n");

    pre_tick = time_us_64();

    while (wls_server.state->complete == false) {

        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
        led_on = !led_on; // Toggle the LED state
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

//        printf("Waiting for work...\n");
        tick = time_us_64();
        // Update the LED color based on the current mode
        wlt_update_rgb_led(prtconfig->net_config.wifi_mode, &rgb_led, tick);

        if ((tick - pre_tick) > prtconfig->data.settings.options.poll_time * 1000000) {
            // If more than poll_time second has passed, we can read the sensor data
            printf("Reading sensor data after %llu microseconds\n", (tick - pre_tick));
            // read the sensor data and update the DB
            if (prtconfig->data.settings.options.sens_avail == SENS_AVAILABLE) {
                if (DHT20_read_data(&(prtconfig->data.temperature), &(prtconfig->data.humidity)) != 0) {
                    printf("Failed to read DHT20 sensor data\n");
                    prtconfig->data.settings.options.data_valid = SENS_DATA_NOT_VALID; // Data not valid
                } else {
                    printf("DHT20 sensor data read successfully: Temperature = %.2f, Humidity = %.2f\n", 
                           prtconfig->data.temperature,
                           prtconfig->data.humidity);
                    prtconfig->data.settings.options.data_valid = SENS_DATA_VALID; // Data valid
                    // Send the data to UART
                    wlt_send_to_uart(prtconfig->data.temperature,
                                     prtconfig->data.humidity,
                                     prtconfig->data.settings.options.t_format,
                                     prtconfig->data.settings.options.out_format);
                }
            } else {
                printf("Sensor not available, using default values\n");
            }
            pre_tick = time_us_64(); // Update the pre_tick to current time 
        }
    }

    if(run_time_config.net_config.wifi_mode == WLT_WIFI_MODE_AP) {
        tcp_server_close(wls_server.state);
        free(wls_server.state);
#if START_DNS_SERVER
        dns_server_deinit(&dns_server);
#endif // START_DNS_SERVER
        dhcp_server_deinit(&dhcp_server);
    }

    cyw43_arch_deinit();
    printf("Exit\n");
    return 0;
}
