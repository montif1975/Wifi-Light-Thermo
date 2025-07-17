#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "include/wlt.h"
#include "include/wlt_global.h"
//#include "include/wlt_tcp.h"

static char *http_req_page_str[HTTP_REQ_MAX] = {
    STYLE_URL,
    STYLE_INFO_URL,
    FAVICON_URL,
    INFO_URL,
    SETTINGS_URL,
    SETTINGS_FORM_URL,
    ADVANCED_URL,
    ADVANCED_FORM_URL,
    SET_HIGH_TEMP_URL,   
    SET_HIGH_TEMP_FORM_URL,
    SET_LOW_TEMP_URL,
    SET_LOW_TEMP_FORM_URL, 
    SET_HIGH_HUM_URL,      
    SET_HIGH_HUM_FORM_URL, 
    SET_LOW_HUM_URL,       
    SET_LOW_HUM_FORM_URL  
};

static char *http_req_api_str[HTTP_API_MAX] = {
    API_GET_INFO_URL,
    API_GET_SETTINGS_URL,
    API_SET_PARAMS_URL,
    API_SET_HIGH_TEMP_URL,
    API_SET_LOW_TEMP_URL, 
    API_SET_HIGH_HUM_URL, 
    API_SET_LOW_HUM_URL
};

/*
 * Function: tcp_close_client_connection()
 * Description: This function closes the TCP client connection and frees the connection state.
 * It returns an error code.
 */
static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err)
{
    if (client_pcb) {
        assert(con_state && con_state->pcb == client_pcb);
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
        if (con_state) {
            free(con_state);
        }
    }
    return close_err;
}

/*
 * Function: tcp_server_close()
 * Description: This function closes the TCP server connection and frees the server state.
 */
void tcp_server_close(TCP_SERVER_T *state)
{
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

/*
 * Function: tcp_server_sent()
 * Description: This function is called when data has been sent to the client.
 * It checks if all data has been sent and closes the connection if so.
 * It returns an error code.
*/
static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_sent %u\n", len);
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        printf("all done\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

/*
 * Function: tcp_server_recv()
 * Description: This function is called when data is received from the client.
 * It processes the received data and sends a response back to the client.
 * It returns an error code.
 */
static int build_req_settings_form(char *result, size_t max_result_len)
{
    // Build the settings form
    // NOTE: Wireshark tell me that this packet is malformed but the browser seems to handle it correctly (no error reported)
    // I see 3 0x00 bytes at the end of the packet, but I don't know why
    int len = 0;
    int len2copy = 0;

    if (prtconfig == NULL) {
        printf("prtconfig is NULL\n");
        return 0; // Error
    }
    if (pconfig == NULL) {
        printf("pconfig is NULL\n");
        return 0; // Error
    }

    len2copy = snprintf(result + len, max_result_len - len, SETTINGS_REPLY_HEAD);        
    if ((len2copy > 0) && (len2copy < max_result_len)) {
        len += len2copy;
    } else {
        printf("Error generating settings head content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }   
    // copy the wifi form
    // we show the wifi SSID used in Station mode, the password is left blank
    // to avoid security issues, the device name is shown as well
    len2copy = snprintf(result + len,
                        max_result_len - len,
                        SETTINGS_REPLY_FORM_WIFI,
                        pconfig->wifi_ssid,
                        WIFI_PASS_HIDDEN,
                        prtconfig->net_config.devicename);
    if ((len2copy > 0) && (len2copy < (max_result_len - len))) {
        len += len2copy;
    } else {
        printf("Error generating settings wifi form content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }
    // copy the sensor form
    len2copy = snprintf(result + len,
                        max_result_len - len,
                        SETTINGS_REPLY_FORM_SENSOR,
                        prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS ? "checked" : "",
                        prtconfig->data.settings.options.t_format == T_FORMAT_FAHRENHEIT ? "checked" : "",
                        prtconfig->data.settings.options.out_format == OUT_FORMAT_TXT ? "checked" : "",
                        prtconfig->data.settings.options.out_format == OUT_FORMAT_CSV ? "checked" : "");
    if ((len2copy > 0) && (len2copy < (max_result_len - len))) {
        len += len2copy;
    } else {
        printf("Error generating settings wifi form content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }
    // copy the footer
    len2copy = strlen(SETTINGS_REPLY_FOOTER);
    if ((len + len2copy) >= max_result_len) {
        printf("Result buffer too small for settings footer (len=%d, len2copy=%d, max_result_len=%zu)\n", len, len2copy, max_result_len);
        return 0; // Error
    }
    else {
        len += snprintf(result + len, max_result_len - len, SETTINGS_REPLY_FOOTER);
    }
    if (len < 0) {
        printf("Error generating settings content\n");
        return 0; // Error
    }
    else {
        printf("Settings content generated successfully, length: %d\n", len);
    }

    return len;
}

/*
 * Function: build_req_adv_settings_form()
 * Description: This function builds the advanced settings form.
 * It returns the length of the generated content or 0 in case of error.
 */
static int build_req_adv_settings_form(char *result, size_t max_result_len)
{
    // Build the advanced settings form
    int len = 0;
    int len2copy = 0;

    if (prtconfig == NULL) {
        printf("prtconfig is NULL\n");
        return 0; // Error
    }

    len2copy = snprintf(result + len, max_result_len - len, ADVANCED_REPLY_HEAD);
    if ((len2copy > 0) && (len2copy < max_result_len)) {
        len += len2copy;
    } else {
        printf("Error generating advparams head content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }

    // copy the sensor config form
    len2copy = snprintf(result + len, max_result_len - len,
                        ADVANCED_REPLY_FORM_SENSOR,
                        POLL_READ_TIME_MIN,
                        POLL_READ_TIME_MAX,
                        prtconfig->data.settings.options.poll_time);
    if ((len2copy > 0) && (len2copy < max_result_len)) {
        len += len2copy;
    } else {
        printf("Error generating advparams content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }

    // copy the thresholds form
    len2copy = snprintf(result + len, max_result_len - len, ADVANCED_REPLY_FORM_THRESHOLDS);
    if ((len2copy > 0) && (len2copy < max_result_len)) {
        len += len2copy;
    } else {
        printf("Error generating advparams head content (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
        return 0; // Error
    }

    // copy the footer
    len2copy = strlen(ADVANCED_REPLY_FOOTER);
    if ((len + len2copy) >= max_result_len) {
        printf("Result buffer too small for settings footer (len=%d, len2copy=%d, max_result_len=%zu)\n", len, len2copy, max_result_len);
        return 0; // Error
    }
    else {
        len += snprintf(result + len, max_result_len - len, ADVANCED_REPLY_FOOTER);
    }
    if (len < 0) {
        printf("Error generating settings content\n");
        return 0; // Error
    }

    return len;
}

/*
 * Function: check_wifi_password()
 * Description: This function checks if the provided Wi-Fi password is valid.
 * It returns 1 if the password is valid, 0 if it's not valid, -1 if it's not change.
 */
static int check_wifi_password(const char *password)
{
    int ret = WIFI_PASS_VALID;

    if (password != NULL) {
        if(strlen(password) == 0) {
            return WIFI_PASS_NOT_CHANGE; // Password not provided, not changed
        }
        // if password is not empty, check if the password is valid (not too long or too short)
        // and not containing special characters
        if (strlen(password) < WIFI_PASS_MIN_LEN || strlen(password) > WIFI_PASS_MAX_LEN) {
            return WIFI_PASS_INVALID; // Invalid password
        }
        for (const char *c = password; *c != '\0'; c++) {
            if (*c < ' ' && *c > '~') { // Check for printable ASCII characters 
                return WIFI_PASS_INVALID; // Invalid password
            }
        }
    }
    else {
        // If the password is NULL, it means it's not changed
        return WIFI_PASS_NOT_CHANGE;
    }

    return ret;
}

/*
 * Function: fix_devname()
 * Description: This function fixes the device name by replacing '%20' or '+' with spaces.
 * It ensures that the device name is properly formatted for display.
 * Parameters:
 * src - the source device name string
 * src_len - the length of the source string
 * dest - the destination buffer to store the fixed device name
 * dest_len - the length of the destination buffer
 */
static void fix_devname(const char *src, size_t src_len, char *dest, size_t dest_len)
{
    // Ensure the destination is empty
    memset(dest, 0, dest_len);
    char *c, *dest_start = dest;

    // Copy the source to the destination, ensuring we don't overflow
    // and replace "%20" or "+" with spaces
    if (src_len >= dest_len) {
        src_len = dest_len - 1; // Leave space for null terminator
    }
    // Replace '%20' or '+' with spaces in the source string
    // This is necessary because when using GET method, spaces in the devicename are encoded
    // as '%20' or '+' in the URL, so we need to replace them with spaces
    for (c = (char *)src; *c && (c - src < src_len); c++) {
        if (*c == '%' && *(c + 1) == '2' && *(c + 2) == '0') {
            *dest++ = ' '; // Replace '%20' with space
            c += 2; // Skip the next two characters
        } else if (*c == '+') {
            *dest++ = ' '; // Replace '+' with space
        } else {
            *dest++ = *c; // Copy the character
        }
    } 
    // Null-terminate the destination string
    if (dest_len > 0) {
        *dest = '\0';
    }
    printf("Fixed devicename: '%s'\n", dest_start);
    return;    
}

/*
 * Function: fill_server_content()
 * Description: This function fills the server content based on the request and parameters.
 * It returns the length of the generated content or 0 in case of error.
 */
static int fill_server_content(const char *request, const char *params, char *result, size_t max_result_len)
{
    int len = 0;
    int len2copy = 0;
    int i = 0;
#if DEBUG
    printf("%s Request: %s?%s\n", __FUNCTION__, request, params);
    printf("max_result_len: %zu\n", max_result_len);
#endif

    if(prtconfig == NULL) {
        printf("prtconfig is NULL\n");
        return 0; // Error
    }
#if DEBUG
    else {
        printf("prtconfig is not NULL\n");
    }
#endif

    // find which request we have
    for (i = 0; i < HTTP_REQ_MAX; i++) {
        if (strncmp(request, http_req_page_str[i], strlen(http_req_page_str[i])) == 0) {
            break;
        }
    }
    if(i < HTTP_REQ_MAX) {
        // We have a page request
        printf("Page request found: %s\n", http_req_page_str[i]);
        switch(i) {
            case HTTP_REQ_STYLE:
                // Copy the style sheet
                len2copy = strlen(STYLE_CSS);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_CSS);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_STYLE_INFO:
                // Copy the style sheet for info page
                len2copy = strlen(STYLE_INFO_CSS);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style info (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_INFO_CSS);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_FAVICON:
                // Copy the favicon icon
                len2copy = favicon_ico_len;
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for favicon (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    memcpy(result, favicon_ico, len2copy);
                    len = len2copy; // Set the length to the length of the favicon
                }
                if (len < 0) {
                    printf("Error generating favicon content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_INFO:
                // copy the info head
                len += snprintf(result + len, max_result_len - len, INFO_REPLY_HEAD);
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                else if (len >= max_result_len) {
                    printf("Result buffer too small for info head (len=%d, max_result_len=%zu)\n", len, max_result_len);
                    return 0; // Error
                }
                // copy the info body
                if (prtconfig->data.settings.options.data_valid == SENS_DATA_NOT_VALID) {
                    // If data is not valid, show a message
                    len += snprintf(result + len, max_result_len - len, INFO_REPLAY_BODY_NOT_VALID, prtconfig->net_config.devicename);
                } else {
                    // Fill in the body with the sensor data
                    len += snprintf(result + len, max_result_len - len, INFO_REPLY_BODY,
                                    prtconfig->net_config.devicename,
                                    (prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS ? prtconfig->data.temperature : C2F(prtconfig->data.temperature)),
                                    (prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS ? "&degC" : "&degF"),
                                    prtconfig->data.humidity);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                else if (len >= max_result_len) {
                    printf("Result buffer too small for info body (len=%d, max_result_len=%zu)\n", len, max_result_len);
                    return 0; // Error
                }
#if DEBUG
                else
                    printf("Info content generated successfully, length: %d\n", len);
#endif
                break;

            case HTTP_REQ_SETTINGS:
#if DEBUG
                // copy the settings form (fill in different step to check the buffer size)
                len = build_req_settings_form(result, max_result_len);
#else
                // this page is available only when in AP mode
                if (prtconfig->net_config.wifi_mode != WLT_WIFI_MODE_AP) {
                    printf("Settings page not available in STA mode\n");
                    len2copy = strlen(SETTINGS_REPLY_NACK);
                    if (len2copy >= max_result_len) {
                        printf("Result buffer too small (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                        return 0; // Error
                    }
                    else {
                        len += snprintf(result + len, max_result_len - len, SETTINGS_REPLY_NACK);
                    }
                }
                else {
                    printf("Settings page requested in AP mode\n");
                    len = build_req_settings_form(result, max_result_len);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
#endif
                break;

            case HTTP_REQ_SETTINGS_FORM:
                // This is the form submission
                if (params) {
                    // Parse the parameters
                    char *ssid = NULL;
                    char *pwd = NULL;
                    char *scale = NULL;
                    char *oform = NULL;
                    char *devname = NULL;

                    // Split params by '&'
                    char *param = strtok((char *)params, "&");
                    while (param) {
                        if (strncmp(param, "ssid=", 5) == 0) {
                            ssid = param + 5; // Skip "ssid="
                        } else if (strncmp(param, "pwd=", 4) == 0) {
                            pwd = param + 4; // Skip "pwd="
                        } else if (strncmp(param, "devname=",8) == 0) {
                            devname = param + 8; // Skip "devname="
                        } else if (strncmp(param, "scale=", 6) == 0) {
                            scale = param + 6; // Skip "scale="
                        } else if (strncmp(param, "oform=", 6) == 0) {
                            oform = param + 6; // Skip "oform="
                        }
                        param = strtok(NULL, "&");
                    }
#if 1 //DEBUG
                    printf("Parsed parameters: ssid=%s, pwd=%s, devname=%s, scale=%s, oform=%s\n", 
                           ssid ? ssid : "NULL", 
                           pwd ? pwd : "NULL", 
                           devname ? devname : "NULL", 
                           scale ? scale : "NULL", 
                           oform ? oform : "NULL");
#endif
                    // Update the configuration
                    if (ssid && pwd && devname && scale && oform) {
                        int ret;
                        strncpy((char *)prtconfig->net_config.wifi_ssid, ssid, sizeof(prtconfig->net_config.wifi_ssid) - 1);
                        ret = check_wifi_password(pwd);
                        if (ret == WIFI_PASS_NOT_CHANGE) {
                            printf("Wi-Fi password not changed\n");
                        } else if (ret == WIFI_PASS_INVALID) {
                            printf("Invalid Wi-Fi password!\n");
                            len = snprintf(result, max_result_len, SETTINGS_SAVE_NACK_EINVAL);
                            return len; // Error
                        } else if (ret == WIFI_PASS_VALID) {
                            printf("Wi-Fi password changed (new password=%s)\n",pwd);
                            // Copy the password, ensuring we don't overflow
                            strncpy((char *)prtconfig->net_config.wifi_pass, pwd, sizeof(prtconfig->net_config.wifi_pass) - 1);
                        }
                        // when use method GET, if the original devicename contains spaces, they are replaced with '%20' or '+' in the URL,
                        // so we need to replace there with spaces
                        fix_devname(devname,strlen(devname),prtconfig->net_config.devicename, sizeof(prtconfig->net_config.devicename));
                        //strncpy((char *)pconfig->net_config.devicename, devname, sizeof(pconfig->net_config.devicename) - 1);
                        prtconfig->data.settings.options.t_format = (strcmp(scale, "C") == 0) ? T_FORMAT_CELSIUS : T_FORMAT_FAHRENHEIT;
                        prtconfig->data.settings.options.out_format = (strcmp(oform, "TXT") == 0) ? OUT_FORMAT_TXT : OUT_FORMAT_CSV;

                        // Save the configuration
                        wlt_update_and_save_config(prtconfig,pconfig);
                        
                        // Prepare success response
                        len = snprintf(result, max_result_len, SETTINGS_SAVE_ACK);
                    } else {
                        len = snprintf(result, max_result_len, SETTINGS_SAVE_NACK_EINVAL);
                    }
                } else {
                    len = snprintf(result, max_result_len, SETTINGS_SAVE_NACK_ENOPARAM);
                }
                if (len >= max_result_len) {
                    printf("Result buffer too small for settings form response (len=%d, max_result_len=%zu)\n", len, max_result_len);
                } 
            break;

            case HTTP_REQ_ADVANCED:
                // copy the advanced settings form (fill in different step to check the buffer size)
                len = build_req_adv_settings_form(result, max_result_len);
                break;
            
            case HTTP_REQ_ADVANCED_FORM:
                // This is the advanced form submission
                if(params) {
                    // Parse the parameters
                    char *ptime = NULL;

                    // Split params by '&'
                    char *param = strtok((char *)params, "&");
                    while (param) {
                        if (strncmp(param, "ptime=", 6) == 0) {
                            ptime = param + 6; // Skip "poll_time="
                        } 
                        param = strtok(NULL, "&");
                    }

                    // Update the configuration
                    if (ptime) {
                        int poll_time_val = atoi(ptime);
 
                        // Validate and update settings
                        if ((poll_time_val >= POLL_READ_TIME_MIN) && (poll_time_val <= POLL_READ_TIME_MAX)) {
                            prtconfig->data.settings.options.poll_time = poll_time_val;
                        }

                        // Save the configuration
                        wlt_update_and_save_config(prtconfig,pconfig);

                        // Prepare success response
                        len = snprintf(result, max_result_len, ADVANCED_SAVE_ACK);
                    }
                    else {
                        len = snprintf(result, max_result_len, ADVANCED_SAVE_NACK_EINVAL);
                    }
                } else {
                    len = snprintf(result, max_result_len, ADVANCED_SAVE_NACK_ENOPARAM);
                }
                if (len >= max_result_len) {
                    printf("Result buffer too small for advanced form response (len=%d, max_result_len=%zu)\n", len, max_result_len);
                }   
                break;

            case HTTP_REQ_SET_HIGH_TEMP:
            case HTTP_REQ_SET_HIGH_TEMP_FORM:
            case HTTP_REQ_SET_LOW_TEMP:
            case HTTP_REQ_SET_LOW_TEMP_FORM:
            case HTTP_REQ_SET_HIGH_HUM:
            case HTTP_REQ_SET_HIGH_HUM_FORM:
            case HTTP_REQ_SET_LOW_HUM:
            case HTTP_REQ_SET_LOW_HUM_FORM:
                len2copy = strlen(REPLY_NOT_YET_IMPLEMENTED);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for info head (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, REPLY_NOT_YET_IMPLEMENTED);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            default:
                printf("Unknown request type %d\n", i);
                // return empty result
                len = 0;
                result[0] = '\0'; // No content for other requests
        }
    } else {
        // Check API requests
        for (i = 0; i < HTTP_API_MAX; i++) {
            if (strncmp(request, http_req_api_str[i], strlen(http_req_api_str[i])) == 0) {
                printf("Request matches API: %s\n", http_req_api_str[i]);
                break;
            }
        }
        if (i < HTTP_API_MAX) {
            // We have an API request
            switch(i) {
                case HTTP_API_INFO:
                    // Generate API info response
                    len2copy = snprintf(result,
                                        max_result_len,
                                        API_INFO_REPLY,
                                        prtconfig->data.temperature,
                                        prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS ? "C" : "F",
                                        prtconfig->data.humidity);
                    if (len2copy >= max_result_len) {
                        printf("Result buffer too small for API info (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                        return 0; // Error
                    }
                    len = len2copy;
                    break;

                case HTTP_API_GET_SETTINGS:
                    char treshold_trigger[5]; // Buffer for threshold trigger
                    // Generate API settings response
                    /*
                        {
                        "WIFI":{
                            "DEVNAME":"studio",
                            "SSID":"FASTWEB",
                            "MODE":"AP",
                            "IPADDR":"192.168.1.63",
                            "NET":"255.255.255.0",
                            "GW":"192.168.1.1"
                        },
                        "PARAMS":{
                            "TF":"C",
                            "OF":"CSV",
                            "PT":30,
                            "TH":3
                        },
                        "THRESH":{
                            "HTT":{
                                "VAL":20,
                                "TR":"H"
                            },
                            "HTH":{
                                "VAL":20,
                                "TR":"H"
                            },
                            "HTP":{
                                "VAL":20,
                                "TR":"H"
                            },
                            "LTT":{
                                "VAL":20,
                                "TR":"L"
                            },
                            "LTH":{
                                "VAL":20,
                                "TR":"L"
                            },
                            "LTP":{
                                "VAL":20,
                                "TR":"NONE"
                            }
                        }
                        }                   
                    */
                    if (prtconfig == NULL) {
                        printf("prtconfig is NULL\n");
                        return 0; // Error
                    }
                    // Start building the JSON response
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "{\"WIFI\":{\"DEVNAME\":\"%s\",\"SSID\":\"%s\",\"MODE\":\"%s\",",
                                    prtconfig->net_config.devicename,
                                    prtconfig->net_config.wifi_ssid,
                                    (prtconfig->net_config.wifi_mode == WLT_WIFI_MODE_AP) ? "AP" : "STA");
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    // Add IP address, netmask, and gateway
#if 1
                    // I believe that I found a bug in snprintf() or in ipaddr_ntoa(),
                    // if I pass all two or three parameters, the sprintf use only the last one
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"IPADDR\":\"%s\",",
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipaddr)));
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"NET\":\"%s\",",
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipmask)));
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"GW\":\"%s\"},",
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.gwaddr)));
#else
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"IPADDR\":\"%s\",\"NET\":\"%s\",\"GW\":\"%s\"},",
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipaddr)),
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.ipmask)),
                                    ipaddr_ntoa((ip4_addr_t *)&(prtconfig->net_config.gwaddr)));
#endif
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    // Add parameters
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"PARAMS\":{\"TF\":\"%s\",\"OF\":\"%s\",\"PT\":%d,\"TH\":%d},",
                                    (prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS) ? "C" : "F",
                                    (prtconfig->data.settings.options.out_format == OUT_FORMAT_TXT) ? "TXT" : "CSV",
                                    prtconfig->data.settings.options.poll_time,
                                    prtconfig->data.settings.options.trd_hyst);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    // Add thresholds high
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.high.temperature.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"THRESH\":{\"HTT\":{\"VAL\":%.02f,\"TR\":\"%s\"},",
                                    prtconfig->data.thresholds.high.temperature.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.high.humidity.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"HTH\":{\"VAL\":%.02f,\"TR\":\"%s\"},",
                                    prtconfig->data.thresholds.high.humidity.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.high.pressure.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"HTP\":{\"VAL\":%.02f,\"TR\":\"%s\"},",
                                    prtconfig->data.thresholds.high.pressure.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    // Add thresholds low
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.low.temperature.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"LTT\":{\"VAL\":%.02f,\"TR\":\"%s\"},",
                                    prtconfig->data.thresholds.low.temperature.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.low.humidity.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"LTH\":{\"VAL\":%.02f,\"TR\":\"%s\"},",
                                    prtconfig->data.thresholds.low.humidity.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    memset(treshold_trigger, 0, sizeof(treshold_trigger));
                    switch(prtconfig->data.thresholds.low.pressure.trigger) {
                        case TRD_TRIGGER_HIGH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "H");
                            break;
                        case TRD_TRIGGER_LOW:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "L");
                            break;
                        case TRD_TRIGGER_BOTH:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "B");
                            break;
                        case TRD_TRIGGER_NONE:
                        default:
                            snprintf(treshold_trigger, sizeof(treshold_trigger), "NONE");
                            break;
                    }
                    len += snprintf(result + len,
                                    max_result_len - len,
                                    "\"LTP\":{\"VAL\":%.02f,\"TR\":\"%s\"}}}",
                                    prtconfig->data.thresholds.low.pressure.value,
                                    treshold_trigger);
                    if ((len < 0) || (max_result_len - len) <= 0) {
                        printf("Error generating info content\n");
                        return 0; // Error
                    }
                    break;

                case HTTP_API_SET_PARAMS:
                    break;

                case HTTP_API_SET_ADVANCED_PARAMS:
                    break;

                case HTTP_API_SET_HIGH_TEMP:
                case HTTP_API_SET_LOW_TEMP:
                case HTTP_API_SET_HIGH_HUM:
                case HTTP_API_SET_LOW_HUM:
                    // Handle setting parameters (not implemented)
                    len = snprintf(result, max_result_len, "{\"status\":\"not_implemented_yet\"}");
                    if (len >= max_result_len) {
                        printf("Result buffer too small for API set params (len=%d, max_result_len=%zu)\n", len, max_result_len);
                        return 0; // Error
                    }
                    break;

                default:
                    printf("Unknown API request type %d\n", i);
                    len = 0; // No content for other requests
                    result[0] = '\0'; // No content for other requests
            }
        } else {
            // No matching request found
            printf("No matching request found\n");
            len = 0; // No content for unknown requests
        }
    }

#if DEBUG
    printf("Generated content length: %d\n", len);
    printf("Content: %s\n", result);
#endif

    return len;
}

/*
 * Function: tcp_server_recv()
 * Description: This function is called when data is received from the client.
 * It processes the received data and sends a response back to the client.
 * It returns an error code.
 */
err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) 
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (!p) {
        printf("connection closed\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    assert(con_state && con_state->pcb == pcb);
    if (p->tot_len > 0) {
        printf("tcp_server_recv %d err %d\n", p->tot_len, err);
#if 0
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            DEBUG_printf("in: %.*s\n", q->len, q->payload);
        }
#endif
        // Copy the request into the buffer
        pbuf_copy_partial(p, con_state->headers, p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len, 0);

        // Handle GET request
        if (strncmp(HTTP_GET, con_state->headers, sizeof(HTTP_GET) - 1) == 0) {
            char *request = con_state->headers + sizeof(HTTP_GET); // + space
            char *params = strchr(request, '?');
            if (params) {
                if (*params) {
                    char *space = strchr(request, ' ');
                    *params++ = 0;
                    if (space) {
                        *space = 0;
                    }
                } else {
                    params = NULL;
                }
            }

            // Generate content
            memset(con_state->result, 0, sizeof(con_state->result));
            con_state->result_len = fill_server_content(request, params, con_state->result, sizeof(con_state->result));

            // Check we had enough buffer space
            if (con_state->result_len > sizeof(con_state->result) - 1) {
                printf("Too much result data %d\n", con_state->result_len);
                // send 500 Internal Server Error
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_INTERNAL_ERROR);
                err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write unsupported request data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }

            // Generate web page
            if (con_state->result_len > 0) {
                if(strstr(request,".css") != NULL) {
                    // If the request is for a CSS file, set content type to text/css
                    con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS, 200, con_state->result_len, "css");
                } else {
                    if(strstr(request, ".ico") != NULL) {
                        // If the request is for a favicon.ico file, set content type to image/x-icon
                        con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS_IMAGE, 200, con_state->result_len, "x-icon");
                    }
                    else if(strstr(request, "/api/") != NULL) {
                        // If the request is an API set content type to application/json
                        con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS_JSON, 200, con_state->result_len,"json");
                    }
                    else
                        // Otherwise, set content type to text/html
                        con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS, 200, con_state->result_len, "html");
                }
                if (con_state->header_len > sizeof(con_state->headers) - 1) {
                    printf("Too much header data %d\n", con_state->header_len);
                    // send 500 Internal Server Error
                    con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_INTERNAL_ERROR);
                    err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                    if (err != ERR_OK) {
                        printf("failed to write unsupported request data %d\n", err);
                        pbuf_free(p);
                        return tcp_close_client_connection(con_state, pcb, err);
                    }
                }
            } else {
                // Send 404 Not Found
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_NOT_FOUND);
                printf("Sending 404 %s", con_state->headers);
                err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write unsupported request data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }

            // Send the headers to the client
            con_state->sent_len = 0;
            err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                printf("failed to write header data %d\n", err);
                pbuf_free(p);
                return tcp_close_client_connection(con_state, pcb, err);
            }

            // Send the body to the client
            if (con_state->result_len) {
                err = tcp_write(pcb, con_state->result, con_state->result_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write result data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }
        }
        else if (strncmp(HTTP_POST, con_state->headers, sizeof(HTTP_POST) - 1) == 0) {
            // Handle POST request
            char *request = con_state->headers + sizeof(HTTP_POST); // + space
            // send 501 Not Implemented Error
            con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_NOT_IMPL_ERROR);
            err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                printf("failed to write unsupported request data %d\n", err);
                pbuf_free(p);
                return tcp_close_client_connection(con_state, pcb, err);
            }
        }
        else {
            // Unsupported request, send 404 Not Found
            con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_NOT_FOUND);
            printf("Unsupported request %s", con_state->headers);
            err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                printf("failed to write unsupported request data %d\n", err);
                pbuf_free(p);
                return tcp_close_client_connection(con_state, pcb, err);
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

/*
 * Function: tcp_server_sent()
 * Description: This function is called when data has been sent to the client.
 * It can be used to handle any post-send actions, such as closing the connection.
 * It returns an error code.
*/
static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_poll_fn\n");
    return tcp_close_client_connection(con_state, pcb, ERR_OK); // Just disconnect clent?
}

/*
 * Function: tcp_server_err()
 * Description: This function is called when an error occurs on the TCP connection.
 * It handles the error and closes the connection if necessary.
 * It returns nothing.
 */
static void tcp_server_err(void *arg, err_t err)
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (err != ERR_ABRT) {
        printf("tcp_client_err_fn %d\n", err);
        tcp_close_client_connection(con_state, con_state->pcb, err);
    }
}

/*
 * Function: tcp_server_accept()
 * Description: This function is called when a new client connection is accepted.
 * It sets up the connection state and registers the necessary callbacks.
 * It returns an error code.
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        printf("failure in accept\n");
        return ERR_VAL;
    }
    printf("client connected\n");

    // Create the state for the connection
    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state) {
        printf("failed to allocate connect state\n");
        return ERR_MEM;
    }
    con_state->pcb = client_pcb; // for checking
    con_state->gw = &state->gw;

    // setup connection to client
    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

/*
 * Function: tcp_server_open()
 * Description: This function opens a TCP server on the specified port.
 * It binds the server to the specified gateway address and starts listening for incoming connections.
 * It returns true if successful, false otherwise.
 */
bool tcp_server_open(void *arg, const char *ap_name)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    printf("starting server on port %d\n", TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        printf("failed to create pcb\n");
        return false;
    }

//    err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    err_t err = tcp_bind(pcb, &state->gw, TCP_PORT);
    if (err) {
        printf("failed to bind to port %d\n",TCP_PORT);
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    printf("Try connecting to '%s'\n", ap_name);
    return true;
}

