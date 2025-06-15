#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "include/wlt.h"
//#include "include/wlt_tcp.h"

// global variables
wlt_run_time_config_t *pconfig;

/*
* Function: wlt_init_run_time_config()
* Description: This function initializes the runtime configuration. 
*/
void wlt_init_run_time_config(wlt_run_time_config_t *config) {
    if (config == NULL) {
        printf("Invalid argument: config is NULL\n");
        return;
    }
    config->net_config.wifi_mode = WLT_WIFI_MODE_STA;
    memset(config->net_config.wifi_ssid, 0, sizeof(config->net_config.wifi_ssid));
    memset(config->net_config.wifi_pass, 0, sizeof(config->net_config.wifi_pass));
    config->data.settings.options.t_format = T_FORMAT_CELSIUS; // Default temperature format is Celsius
    config->data.settings.options.out_format = OUT_FORMAT_CSV; // Default output format is CSV
    config->data.settings.options.poll_time = 5; // Default poll time is 5 seconds
    config->data.temperature = 0.0f; // Initialize temperature
    config->data.humidity = 0.0f; // Initialize humidity
    config->data.pressure = 0.0f; // Initialize pressure

    return;
}

/*
* Function: wlt_configure_gpio_select_wifi_mode()
* Description: This function configures the GPIO to select the wifi mode.
 */
void wlt_configure_gpio_select_wifi_mode() {
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
*   Main function
*/
int main()
{
    int ret;
    wlt_run_time_config_t run_time_config;
    dhcp_server_t dhcp_server;
    dns_server_t dns_server;
    wls_server_t wls_server;
    char led_on = 1;

    pconfig = &run_time_config;
    srand(to_us_since_boot(get_absolute_time()));
    stdio_init_all();

    sleep_ms(2000);

    wlt_init_run_time_config(&run_time_config);

    // configure the GPIO to select the wifi mode.
    wlt_configure_gpio_select_wifi_mode();

    // check the GPIO to select the wifi mode.
    ret = wlt_check_wifi_mode(&run_time_config);
    if (ret != WLT_SUCCESS) {
        printf("failed to check the wifi mode\n");
        return -1;
    } else {
        if(run_time_config.net_config.wifi_mode == WLT_WIFI_MODE_STA) {
            printf("Wi-Fi mode: STA\n");
            // TODO: Add a mechanism to read the SSID and password from a file or other source
            // For now, we use hardcoded values for the SSID and password
            strncpy(run_time_config.net_config.wifi_ssid, WIFI_SSID, sizeof(run_time_config.net_config.wifi_ssid));
            strncpy(run_time_config.net_config.wifi_pass, WIFI_PASS, sizeof(run_time_config.net_config.wifi_pass));
        } else {
            printf("Wi-Fi mode: AP\n");
            // in AP mode we use hardcoded values for the SSID and password
            strncpy(run_time_config.net_config.wifi_ssid, WIFI_AP_SSID, sizeof(run_time_config.net_config.wifi_ssid));
            strncpy(run_time_config.net_config.wifi_pass, WIFI_AP_PASS, sizeof(run_time_config.net_config.wifi_pass));
        }
    }

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    // start switching on the LED        
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

    if(run_time_config.net_config.wifi_mode == WLT_WIFI_MODE_AP) {
        // Enable wifi access point
        cyw43_arch_enable_ap_mode(run_time_config.net_config.wifi_ssid, run_time_config.net_config.wifi_pass,CYW43_AUTH_WPA2_AES_PSK);

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
        // Enable wifi station
        cyw43_arch_enable_sta_mode();

        printf("Connecting to Wi-Fi...\n");
        if (cyw43_arch_wifi_connect_timeout_ms(run_time_config.net_config.wifi_ssid, run_time_config.net_config.wifi_pass, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("failed to connect.\n");
            // TODO: Add a retry mechanism
            // or swith to AP mode; blink the LED to indicate the error?
            return -1;
        } else {
            printf("Connected.\n");
            run_time_config.net_config.ipaddr = cyw43_state.netif[0].ip_addr.addr;
            run_time_config.net_config.ipmask = cyw43_state.netif[0].netmask.addr; 
            run_time_config.net_config.gwaddr = cyw43_state.netif[0].gw.addr;
            // Set the wls_server ip_addr and ip_mask
            wls_server.ip_addr.addr = cyw43_state.netif[0].ip_addr.addr;
            wls_server.ip_mask.addr = cyw43_state.netif[0].netmask.addr;
            printf("IP address (cyw43) )0x%X\n", cyw43_state.netif[0].ip_addr.addr);
            printf("IP address (wls_server) 0x%X\n", wls_server.ip_addr.addr);
            // Read the ip address in a human readable way
            printf("IP address %s\n", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr));
            printf("IP mask %s\n", ipaddr_ntoa(&cyw43_state.netif[0].netmask));
            printf("Gateway %s\n", ipaddr_ntoa(&cyw43_state.netif[0].gw));            
        }
    }

    // Start the tcp server
    wls_server.state = (TCP_SERVER_T *)calloc(1, sizeof(TCP_SERVER_T));
    if (!wls_server.state) {
        printf("Fatal error: failed to allocate wls_server.state\n");
        return -1;
    }
    wls_server.state->server_pcb = NULL;
    wls_server.state->complete = false;
    wls_server.state->gw.addr = wls_server.ip_addr.addr;

    if (!tcp_server_open(wls_server.state, run_time_config.net_config.wifi_ssid)) {
        printf("Failed to open server\n");
        return -1;
    }
    printf("Server opened successfully\n");

    while (wls_server.state->complete == false) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
        led_on = !led_on; // Toggle the LED state
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

//        printf("Waiting for work...\n");
        // read the sensor data and update the DB
        // TODO: Add a mechanism to read the sensor data
        run_time_config.data.temperature = 25.0f + ((rand() % 5) - 2); // Example temperature
        run_time_config.data.humidity = 50.0f + ((rand() % 5) - 2); // Example humidity
//        printf("Temperature: %.2f, Humidity: %.2f\n", run_time_config.data.temperature, run_time_config.data.humidity); 
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
