#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "include/wlt.h"
#include "include/wlt_global.h"
#include "json/ecjp.h"

extern wlt_error_t parse_post_specific_body(char *body, int api_index);
extern wlt_error_t parse_post_body(char *body, size_t content_length);

static char *http_get_req_str[HTTP_GET_REQ_MAX] = {
    HTTP_NONE_URL,
    STYLE_URL,
    STYLE_FORM_DARK_URL,
    STYLE_FORM_LIGHT_URL,
    STYLE_INFO_URL,
    STYLE_HOME_DARK_URL,
    STYLE_HOME_LIGHT_URL,
    HOME_URL,
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
    SET_LOW_HUM_FORM_URL,  
    API_GET_INFO_URL,
    API_GET_SETTINGS_URL
};

static char *http_post_req_str[HTTP_POST_REQ_MAX] = {
    API_SET_ALL_PARAMS_URL,
    API_SET_WIFI_PARAMS_URL,
    API_SET_SETTING_PARAMS_URL, 
    API_SET_THRESH_PARAMS_URL
};

/*
* Function: get_path()
* Description: This function extracts the path from an HTTP request string.
*/
int get_path(const char *req, const char **path_start, size_t *path_len)
{
    const char *start = req;
    if (!start) return -1;

    const char *end = strchr(start, ' ');
    if (!end) return -1;

    *path_start = start;
    *path_len = end - start;

    return 0;
}

/*
* Function: tcp_find_get_request()
* Description: This function identifies the type of HTTP GET request based on the path in the request string.
* It returns the corresponding enum value for the request type, or HTTP_GET_REQ_MAX if the request type is not recognized.
*/
enum http_get_req tcp_find_get_request(const char *req)
{
    enum http_get_req page;
    const char *path;
    size_t len;

    if (get_path(req, &path, &len) != 0)
        return HTTP_GET_REQ_MAX;

    for (page = 1; page < HTTP_GET_REQ_MAX; page++) {
        const char *ref = http_get_req_str[page];
        size_t ref_len = strlen(ref);

        if (len == ref_len && strncmp(path, ref, len) == 0) {
            return page;
        }
    }
    return HTTP_GET_REQ_MAX;
}

/*
 * Function: tcp_find_post_request()
 * Description: This function identifies the type of HTTP POST request based on the path in the request string.
 * It returns the corresponding enum value for the request type, or HTTP_POST_REQ_MAX if the request type is not recognized.
*/
enum http_post_req tcp_find_post_request(const char *req)
{
    enum http_post_req api;
    const char *path;
    size_t len;

    if (get_path(req, &path, &len) != 0)
        return HTTP_POST_REQ_MAX;

    for (api = 0; api < HTTP_POST_REQ_MAX; api++) {
        const char *ref = http_post_req_str[api];
        size_t ref_len = strlen(ref);

        if (len == ref_len && strncmp(path, ref, len) == 0) {
            return api;
        }
    }
    return HTTP_POST_REQ_MAX;
}

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

    if (prtconfig->data.settings.options.theme == THEME_DARK) {
        len2copy = snprintf(result + len, max_result_len - len, SETTINGS_REPLY_HEAD, STYLE_FORM_DARK_URL);
    } else {
        len2copy = snprintf(result + len, max_result_len - len, SETTINGS_REPLY_HEAD, STYLE_FORM_LIGHT_URL);
    }
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
 * Function: fill_server_content()
 * Description: This function fills the server content based on the request and parameters.
 * It returns the length of the generated content or 0 in case of error.
 */
static int fill_server_content(const char *request, const char *params, char *result, size_t max_result_len)
{
    int len = 0;
    int len2copy = 0;
    int i = 0;
    char treshold_trigger[5]; // Buffer for threshold trigger
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
    i = tcp_find_get_request(request);
    if(i < HTTP_GET_REQ_MAX) {
        // We have a page request
        printf("Page request found: %s\n", http_get_req_str[i]);
        switch(i) {
            case HTTP_NONE:
                break;

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

            case HTTP_REQ_STYLE_FORM_DARK:
                // Copy the style sheet
                len2copy = strlen(STYLE_FORM_DARK);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_FORM_DARK);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_STYLE_FORM_LIGHT:
                // Copy the style sheet for settings form
                len2copy = strlen(STYLE_FORM_LIGHT);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style form (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_FORM_LIGHT);
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

            case HTTP_REQ_STYLE_DARK:
                // Copy the style sheet
                len2copy = strlen(STYLE_CSS_DARK);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_CSS_DARK);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_STYLE_LIGHT:
                // Copy the style sheet
                len2copy = strlen(STYLE_CSS_LIGHT);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for style (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, STYLE_CSS_LIGHT);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                break;

            case HTTP_REQ_HOME:
                // copy the info head
                if (prtconfig->data.settings.options.theme == THEME_DARK) {
                    len += snprintf(result + len, max_result_len - len, HOME_REPLY_HEAD, "style_dark.css");
                }
                else {
                    len += snprintf(result + len, max_result_len - len, HOME_REPLY_HEAD, "style_light.css");
                }
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
                    len += snprintf(result + len, max_result_len - len, HOME_REPLY_BODY_NOT_VALID, prtconfig->net_config.devicename);
                } else {
                    // Fill in the body with the sensor data
                    len += snprintf(result + len, max_result_len - len, HOME_REPLY_BODY,
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
                    len += snprintf(result + len, max_result_len - len, INFO_REPLY_BODY_NOT_VALID, prtconfig->net_config.devicename);
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
#if 1 //DEBUG
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
#if DEBUG
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
                    "SETTINGS":{
                        "TF":"C",
                        "OF":"CSV",
                        "PT":30,
                        "TH":3,
                        "WT":"DARK"
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
                                "\"SETTINGS\":{\"TF\":\"%s\",\"OF\":\"%s\",\"PT\":%d,\"TH\":%d,\"WT\":\"%s\"},",
                                (prtconfig->data.settings.options.t_format == T_FORMAT_CELSIUS) ? "C" : "F",
                                (prtconfig->data.settings.options.out_format == OUT_FORMAT_TXT) ? "TXT" : "CSV",
                                prtconfig->data.settings.options.poll_time,
                                prtconfig->data.settings.options.trd_hyst,
                                (prtconfig->data.settings.options.theme == THEME_DARK) ? "DARK" : "LIGHT");
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

            default:
                printf("Unknown request type %d\n", i);
                // return empty result
                len = 0;
                result[0] = '\0'; // No content for other requests
        }
    } else {
        // Check API (POST) requests
        i = tcp_find_post_request(request);
        if (i < HTTP_POST_REQ_MAX) {
            // We have an API request
            switch(i) {
                case HTTP_API_SET_ALL_PARAMS:
                case HTTP_API_SET_WIFI_PARAMS:
                case HTTP_API_SET_SETTING_PARAMS:
                case HTTP_API_SET_THRESH_PARAMS:
                    len = snprintf(result, max_result_len, "{\"status\":\"ok\"}");
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
            int http_req_index = -1;
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

            printf("Received request: %s\n", request);

            http_req_index = tcp_find_get_request(request);
            if (http_req_index == 0) {
                // FIXME: provare 
                printf("No Request, redirect to home page\n");
                // send 302 Redirect
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_REDIRECT, con_state->gw);
                err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write unsupported request data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
                pbuf_free(p);
                return ERR_OK;
            } else if (http_req_index < HTTP_GET_REQ_MAX) {
                printf("Request matches page: %s\n", http_get_req_str[http_req_index]);
            } else {
                printf("Unsupported HTTP request: %s (http_req_index: %d)\n", request, http_req_index);
                // send 404 Not Found
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_NOT_FOUND);
                err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write unsupported request data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
                pbuf_free(p);
                return ERR_OK;
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
            char *params = NULL;
            char parse_result = false;
            int api_index = -1;

            api_index = tcp_find_post_request(request);
            if (api_index < HTTP_POST_REQ_MAX) {
                printf("Request matches POST: %s\n", http_post_req_str[api_index]);
            } else {
                printf("Unsupported POST request: %s\n", request);
                // send 404 Not Found
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_NOT_FOUND);
                err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write unsupported request data %d\n", err);
                    pbuf_free(p);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
                pbuf_free(p);
                return ERR_OK;
            }

            char *content_length_ptr = strstr(p->payload, "Content-Length:");
            if (content_length_ptr != NULL) {
                // We have content length, so we can read the body
                char *content_length_str = content_length_ptr + 15;
                int content_length = atoi(content_length_str);
                printf("Content-Length: %d\n", content_length);
                // Here we would normally read the body and process it
                // search /r/n/r/n to find the end of headers
                char *body = strstr(p->payload, "\r\n\r\n");
                if (body) {
                    body += 4; // skip the \r\n\r\n
                    int body_length = p->tot_len - (body - (char *)p->payload);
                    printf("Body length: %d\n", body_length);
                    if (body_length >= content_length) {
                        // Null-terminate the body for safety
                        body[content_length] = 0;
                        // We have the full body
                        printf("Body: %.*s\n", content_length, body);
                        // Here we would process the body content depending on the API
                        switch (api_index)
                        {
                            case HTTP_API_SET_WIFI_PARAMS:
                                // in this case we expect ONLY a specific type of parameters in the body
                                parse_result = parse_post_specific_body(body, PARAMS_WIFI);
                                break;

                            case HTTP_API_SET_SETTING_PARAMS:
                                // in this case we expect ONLY a specific type of parameters in the body
                                parse_result = parse_post_specific_body(body, PARAMS_SETTINGS);
                                break;

                            case HTTP_API_SET_THRESH_PARAMS:
                                // in this case we expect ONLY a specific type of parameters in the body
                                parse_result = parse_post_specific_body(body, PARAMS_THRESHOLDS);
                                break;

                            case HTTP_API_SET_ALL_PARAMS:
                                // in this case we need to parse all parameters that are in the body
                                printf("Processing SET_ALL_PARAMS API request\n");
                                parse_result = parse_post_body(body, content_length);
                                break;

                            default:
                                printf("Unknown API POST request\n");
                                parse_result = WLT_GENERIC_ERROR;
                                break;
                        }
                    } else {
                        printf("Incomplete body received\n");
                        parse_result = WLT_GENERIC_ERROR;
                    }
                } else {
                    printf("No body found in POST request\n");
                    parse_result = WLT_GENERIC_ERROR;
                }
            }
            else {
                printf("No Content-Length header found in POST request\n");
                parse_result = WLT_GENERIC_ERROR;
            }

            if (parse_result == WLT_SUCCESS) {

                // Save the configuration
                wlt_update_and_save_config(prtconfig,pconfig);

                // Generate content reply
                memset(con_state->result, 0, sizeof(con_state->result));
                printf("Filling server content for request: %s with params: %s\n", request, params ? params : "NULL");
                con_state->result_len = fill_server_content(request, params, con_state->result, sizeof(con_state->result));
                // send 200 OK
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS_JSON, 200, con_state->result_len,"json");
            } else {
                // send 400 Bad Request
                con_state->result_len = 0;
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_BAD_REQUEST);
            }   
            err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                printf("failed to write unsupported request data %d\n", err);
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

