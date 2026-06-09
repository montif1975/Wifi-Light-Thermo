#include "include/wlt.h"
#include "include/wlt_global.h"
#include "json/ecjp.h"

// forward declarations of API parse functions
int check_and_remove_quotes(char *str);
wlt_error_t api_parse_wifi(char *wifi_params);
wlt_error_t api_parse_settings(char *settings_params);
wlt_error_t api_parse_outputs(char *outputs_params);
wlt_error_t api_parse_single_output(char *output_params, int output_index);

api_parse_key_t api_parse_keys[PARAMS_MAX] = {
    {"WIFI", api_parse_wifi},
    {"SETTINGS", api_parse_settings},
    {"OUTS", api_parse_outputs}
};

static char *api_wifi_params[WIFI_PARAM_MAX] = {
    "DEVNAME",
    "MODE",
    "SSID",
    "PASS"
};

static char *api_settings_params[SETTINGS_MAX] = {
    "TF",
    "OF",
    "PT",
    "TH",
    "WT"
};

static char *api_outs_params[OUTPUTS_MAX] = {
    "GPIO",
    "DT",
    "TH",
    "TR"
};

static char *threshold_trigger_types[TRD_TRIGGER_MAX] = {
    "NONE",
    "H",
    "L"
};

/*
 * Function: check_and_remove_quotes()
 * Description: This function checks if a string is enclosed in double quotes
 *              and removes them if present.
 * Parameters:
 * str - pointer to the string to be checked and modified
 * Returns:
 * 1 if quotes were removed, 0 otherwise
*/
int check_and_remove_quotes(char *str)
{
    size_t len = strlen(str);
    if (!str)
        return 0;

    if (len >= 2 && str[0] == '\"' && str[len - 1] == '\"')
    {
        for (size_t i = 0; i < len - 2; i++)
        {
            str[i] = str[i + 1];
        }
        for (size_t i = len - 2; i < len; i++)
        {
            str[i] = '\0';
        }
        return 1;
    }
    return 0;
}

/*
 * Function: api_parse_wifi()
 * Description: This function parses the WIFI parameters from a JSON string.
 * Parameters:
 * wifi_params - pointer to the JSON string containing WIFI parameters
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_wifi(char *wifi_params)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;
    
    memset(&results, 0, sizeof(results));

    printf("Parsing WIFI parameters...\n");
    ret = ecjp_check_and_load_2(wifi_params, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing settings form data: %d\n", ret);
        res = WLT_GENERIC_ERROR;
    } else {
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                case ECJP_TYPE_ARRAY:
                    printf("Item Object and Array not supported.\n");
                    res = WLT_GENERIC_ERROR;
                    break;
                
                case ECJP_TYPE_KEY_VALUE_PAIR:
                    memset(key, 0, sizeof(key));
                    memset(value, 0, sizeof(value));
                    // for MCUs without sscanf support, use library function
                    if (ecjp_split_key_and_value(item_list, key, value, ECJP_BOOL_FALSE) != ECJP_NO_ERROR) {
                        printf("Failed to split key-value pair.\n");
                        res = WLT_GENERIC_ERROR;
                    } else  {
                        check_and_remove_quotes(value);
                        printf("Key = '%s', Value = '%s'\n", key, value);
                        for (i=0; i<WIFI_PARAM_MAX && res == WLT_SUCCESS; i++) {
                            if (strcmp(key, api_wifi_params[i]) == 0) {
                                printf("Found key '%s', parsing...\n", key);
                                switch (i) {
                                    case WIFI_DEVICENAME:
                                        printf("Setting WIFI Device Name to '%s'\n", value);
                                        // set device name
                                        memset(prtconfig->net_config.devicename, 0, sizeof(prtconfig->net_config.devicename));
                                        strncpy(prtconfig->net_config.devicename, value, sizeof(prtconfig->net_config.devicename) - 1);
                                        break;

                                    case WIFI_MODE:
                                        // set wifi mode
                                        printf("Setting WIFI Mode to '%s'\n", value);
                                        if (strcmp(value, "AP") == 0) {
                                            // AP mode
                                            prtconfig->net_config.wifi_mode = WLT_WIFI_MODE_AP;
                                        } else if (strcmp(value, "STA") == 0) {
                                            // STA mode
                                            prtconfig->net_config.wifi_mode = WLT_WIFI_MODE_STA;
                                        } else {
                                            printf("Invalid WIFI mode value: %s\n", value);
                                            res = WLT_INVALID_ARGUMENT;
                                        }
                                        break;

                                    case WIFI_SSID:
                                        printf("Setting WIFI SSID to '%s'\n", value);
                                        // set wifi ssid
                                        memset(prtconfig->net_config.wifi_ssid, 0, sizeof(prtconfig->net_config.wifi_ssid));
                                        strncpy(prtconfig->net_config.wifi_ssid, value, sizeof(prtconfig->net_config.wifi_ssid) - 1);
                                        break;

                                    case WIFI_PASS:
                                        printf("Setting WIFI PASS to '%s'\n", value);
                                        // set wifi password after checking its validity
                                        if (check_wifi_password(value) != WIFI_PASS_VALID) {
                                            printf("Invalid WIFI password.\n");
                                            res = WLT_INVALID_ARGUMENT;
                                            break;
                                        } else {
                                            printf("WIFI password is valid.\n");
                                            memset(prtconfig->net_config.wifi_pass, 0, sizeof(prtconfig->net_config.wifi_pass));
                                            strncpy(prtconfig->net_config.wifi_pass, value, sizeof(prtconfig->net_config.wifi_pass) - 1);
                                        }
                                        break;

                                    default:
                                        printf("Unknown WIFI parameter.\n");
                                        res = WLT_GENERIC_ERROR;
                                        break;
                                }
                            }
                        }
                    }
                    break;
                
                default:
                    printf("Unknown type.\n");
                    res = WLT_GENERIC_ERROR;
                    break;
            }
            item_list = item_list->next;

            if (res != WLT_SUCCESS) {
                break;
            }
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}

/*
 * Function: api_parse_settings()
 * Description: This function parses the SETTINGS parameters from a JSON string.
 * Parameters:
 * settings_params - pointer to the JSON string containing SETTINGS parameters
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_settings(char *settings_params)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;
    
    memset(&results, 0, sizeof(results));

    printf("Parsing SETTINGS parameters...\n");
    ret = ecjp_check_and_load_2(settings_params, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing settings form data: %d\n", ret);
        res = WLT_GENERIC_ERROR;
    } else {
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                case ECJP_TYPE_ARRAY:
                    printf("Item Object and Array not supported.\n");
                    res = WLT_GENERIC_ERROR;
                    break;
                
                case ECJP_TYPE_KEY_VALUE_PAIR:
                    memset(key, 0, sizeof(key));
                    memset(value, 0, sizeof(value));
                    // for MCUs without sscanf support, use library function
                    if (ecjp_split_key_and_value(item_list, key, value, ECJP_BOOL_FALSE) != ECJP_NO_ERROR) {
                        printf("Failed to split key-value pair.\n");
                        res = WLT_GENERIC_ERROR;
                    } else  {
                        check_and_remove_quotes(value);
                        printf("Key = '%s', Value = '%s'\n", key, value);
                        for (i=0; i<SETTINGS_MAX && res == WLT_SUCCESS; i++) {
                            if (strcmp(key, api_settings_params[i]) == 0) {
                                printf("(i=%d) Found key '%s', parsing...\n", i, key);
                                switch (i) {
                                    case SETTINGS_TEMP_FORMAT:
                                        printf("Setting Temperature Format to '%s'\n", value);
                                        // set temperature format
                                        if (strcmp(value, "C") == 0) {
                                            prtconfig->data.settings.options.t_format = T_FORMAT_CELSIUS;
                                        } else if (strcmp(value, "F") == 0) {
                                            prtconfig->data.settings.options.t_format = T_FORMAT_FAHRENHEIT;
                                        } else {
                                            printf("Invalid Temperature Format value: %s\n", value);
                                            res = WLT_INVALID_ARGUMENT;
                                        }
                                        break;

                                    case SETTINGS_OUTPUT_FORMAT:
                                        printf("Setting Output Format to '%s'\n", value);
                                        // set output format
                                        if (strcmp(value, "CSV") == 0) {
                                            prtconfig->data.settings.options.out_format = OUT_FORMAT_CSV;
                                        } else if (strcmp(value, "TXT") == 0) {
                                            prtconfig->data.settings.options.out_format = OUT_FORMAT_TXT;
                                        } else {
                                            printf("Invalid Output Format value: %s\n", value);
                                            res = WLT_INVALID_ARGUMENT;
                                        }
                                        break;

                                    case SETTINGS_POLL_TIME:
                                        {
                                            int poll_time = atoi(value);
                                            printf("Setting Poll Time to '%d'\n", poll_time);
                                            // set poll time
                                            if ((poll_time >= POLL_READ_TIME_MIN) && (poll_time <= POLL_READ_TIME_MAX)) {
                                                prtconfig->data.settings.options.poll_time = poll_time;
                                            } else {
                                                printf("Invalid Poll Time value: %d\n", poll_time);
                                                res = WLT_INVALID_ARGUMENT;
                                            }
                                        }
                                        break;

                                    case SETTINGS_TRD_HYSTERIS:
                                        {
                                            int trd_hyst = atoi(value);
                                            printf("Setting Threshold Hysteresis to '%d'\n", trd_hyst);
                                            // set threshold hysteresis
                                            if ((trd_hyst >= 0) && (trd_hyst <= 7)) { // 3 bits
                                                prtconfig->data.settings.options.trd_hyst = trd_hyst;
                                            } else {
                                                printf("Invalid Threshold Hysteresis value: %d\n", trd_hyst);
                                                res = WLT_INVALID_ARGUMENT;
                                            }
                                        }
                                        break;

                                    case SETTINGS_THEME:
                                        printf("Setting Theme to '%s'\n", value);
                                        // set theme
                                        if (strcmp(value, "DARK") == 0) {
                                            prtconfig->data.settings.options.theme = THEME_DARK;
                                        } else if (strcmp(value, "LIGHT") == 0) {
                                            prtconfig->data.settings.options.theme = THEME_LIGHT;
                                        } else {
                                            printf("Invalid Theme value: %s\n", value);
                                            res = WLT_INVALID_ARGUMENT;
                                        }
                                        break;

                                    default:
                                        printf("Unknown SETTINGS parameter.\n");
                                        res = WLT_GENERIC_ERROR;
                                        break;  
                                }
                            }
                        }
                    }
                    break;
                
                default:
                    printf("Unknown type.\n");
                    res = WLT_GENERIC_ERROR;
                    break;
            }
            item_list = item_list->next;

            if (res != WLT_SUCCESS) {
                break;
            }
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}

/*
 * Function: api_parse_outputs()
 * Description: This function parses the OUTPUTS parameters from a JSON string.
 * Parameters:
 * outputs_params - pointer to the JSON string containing OUTPUTS parameters
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_outputs(char *outputs_params)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;
    
    memset(&results, 0, sizeof(results));

    printf("Parsing OUTPUTS parameters: %s\n", outputs_params);
    ret = ecjp_check_and_load_2(outputs_params, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing outputs form data: %d\n", ret);
        res = WLT_GENERIC_ERROR;
    } else {
        res = WLT_SUCCESS;
        if(results.struct_type != ECJP_ST_ARRAY) {
            printf("Expected an array of outputs parameters.\n");
            res = WLT_GENERIC_ERROR;
        } else {
            i = 0;
            while (item_list != NULL) {
                printf("Output %d:\n", (i + 1));
                printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
                switch (item_list->item.type) {
                    case ECJP_TYPE_OBJECT:
                        if(api_parse_single_output((char *)item_list->item.value, i) != WLT_SUCCESS) {
                            printf("Failed to parse output %d parameters.\n", (i + 1));
                            res = WLT_GENERIC_ERROR;
                        }
                        else {
                            printf("Parsed output %d parameters successfully.\n", (i + 1));
                            res = WLT_SUCCESS;
                        }
                        break;

                    case ECJP_TYPE_ARRAY:
                        printf("Item Object and Array not supported.\n");
                        res = WLT_GENERIC_ERROR;
                        break;
                    
                    case ECJP_TYPE_KEY_VALUE_PAIR:
                        printf("Item Key-Value Pair not supported.\n");
                        res = WLT_GENERIC_ERROR;
                        break;
                    
                    default:
                        printf("Unknown type.\n");
                        res = WLT_GENERIC_ERROR;
                        break;
                }
                item_list = item_list->next;

                if (res != WLT_SUCCESS) {
                    break;
                }
                i++;
                if (i >= OUTPUT_GPIO_MAX) {
                    printf("Maximum number of outputs exceeded. Only the first %d outputs will be processed.\n", OUTPUT_GPIO_MAX);
                    break;
                }
            }
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}

/*
 * Function: api_parse_single_output()
 * Description: This function parses the parameters of a single output from a JSON string.
 * Parameters:
 * output_params - pointer to the JSON string containing the parameters of a single output
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_single_output(char *output_params, int output_index)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;
    
    res = WLT_SUCCESS;
    results.err_pos = -1;
    results.num_keys = 0;
    results.struct_type = ECJP_ST_NULL;
    results.memory_used = 0;

    ret = ecjp_load_2(output_params,&item_list,&results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing outputs form data: %d\n", ret);
    } else {
        while (item_list != NULL) {
            printf("  Item read successfully.\n");
            printf("  Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            if(item_list->item.type == ECJP_TYPE_KEY_VALUE_PAIR) {
                // parse key-value pair
                memset(key, 0, sizeof(key));
                memset(value, 0, sizeof(value));
                // for MCUs without sscanf support, use library function
                if (ecjp_split_key_and_value(item_list, key, value, ECJP_BOOL_FALSE) != ECJP_NO_ERROR) {
                    printf("Failed to split key-value pair.\n");
                    res = WLT_GENERIC_ERROR;
                } else  {
                    check_and_remove_quotes(value);
                    printf("Key = '%s', Value = '%s'\n", key, value);
                    for (i=0; i<OUTPUTS_MAX && res == WLT_SUCCESS; i++) {
                        if (strcmp(key, api_outs_params[i]) == 0) {
                            printf("Found key '%s', parsing...\n", key);
                            switch (i) {
                                case OUTPUTS_GPIO:
                                    printf("Setting Output GPIO to '%s'\n", value);
                                    // check if gpio is valid
                                    int gpio_num = atoi(value);
                                    // TODO: for better validation, check if the GPIO is already used by other output and check
                                    // if it's not used by other functions (e.g. I2C, SPI, UART) depending on the board pinout
                                    if ((gpio_num >= 0) && (gpio_num <= 26)) { // check if gpio is valid for Raspberry Pi Pico
                                        prtconfig->data.outputs[output_index].gpio_num = gpio_num;
                                    } else {
                                        printf("Invalid GPIO value: %d\n", gpio_num);
                                        res = WLT_INVALID_ARGUMENT;
                                    }
                                    break;

                                case OUTPUTS_DATA_TYPE:
                                    printf("Setting Output Data Type to '%s'\n", value);
                                    // set data type
                                    if (strcmp(value, "T") == 0) {
                                        prtconfig->data.outputs[output_index].data_type = WLT_DATA_TYPE_TEMP;
                                    } else if (strcmp(value, "H") == 0) {
                                        prtconfig->data.outputs[output_index].data_type = WLT_DATA_TYPE_HUMIDITY;
                                    } else if (strcmp(value, "P") == 0) {
                                        prtconfig->data.outputs[output_index].data_type = WLT_DATA_TYPE_PRESSURE;
                                    } else if (strcmp(value, "NULL") == 0) {
                                        prtconfig->data.outputs[output_index].data_type = WLT_DATA_TYPE_NULL;
                                    } else {
                                        printf("Invalid Data Type value: %s\n", value);
                                        res = WLT_INVALID_ARGUMENT;
                                    }
                                    break;

                                case OUTPUTS_THRESHOLD:
                                    printf("Setting Output Threshold to '%s'\n", value);
                                    // set threshold
                                    float threshold = strtof(value, NULL);
                                    prtconfig->data.outputs[output_index].threshold = threshold;
                                    printf("Output Threshold set to %0.2f\n", threshold);
                                    break;

                                case OUTPUTS_TRIGGER:
                                    printf("Setting Output Trigger to '%s'\n", value);
                                    // set trigger
                                    if (strcmp(value, "H") == 0) {
                                        prtconfig->data.outputs[output_index].trigger = TRD_TRIGGER_HIGH;
                                    } else if (strcmp(value, "L") == 0) {
                                        prtconfig->data.outputs[output_index].trigger = TRD_TRIGGER_LOW;
                                    } else if (strcmp(value, "NONE") == 0) {
                                        prtconfig->data.outputs[output_index].trigger = TRD_TRIGGER_NONE;
                                    } else {
                                        printf("Invalid Trigger value: %s\n", value);
                                        res = WLT_INVALID_ARGUMENT;
                                    }
                                    break;

                                default:
                                    printf("Unknown OUTPUTS parameter.\n");
                                    res = WLT_GENERIC_ERROR;
                                    break;
                            }
                        }
                    }
                }
            }
            item_list = item_list->next;
        }
    }
    ecjp_free_item_list(&item_list);

    return res;
}


/*
 * Function: parse_post_specific_body()
 * Description: This function parses the specific form data received from the client calling the parse function associated with the API index.
 * Parameters:
 * body - pointer to the body of the HTTP request
 * api_index - index of the API being called
*/
wlt_error_t parse_post_specific_body(char *body, int api_index)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;

    res = WLT_GENERIC_ERROR;
    if (api_index < 0 || api_index >= PARAMS_MAX) {
        printf("Invalid API index: %d\n", api_index);
        res = WLT_INVALID_ARGUMENT;
        return res;
    }

    // Parse the form data using ecjp
    ret = ecjp_check_and_load_2(body, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing settings form data: %d\n", ret);
        ecjp_free_item_list(&item_list);
        return WLT_GENERIC_ERROR;
    }
    else {
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                case ECJP_TYPE_ARRAY:
                    printf("Parsing Object and Array not supported\n");
                    break;
                
                case ECJP_TYPE_KEY_VALUE_PAIR:
                    printf("Parsing key-value pair...\n");
                    memset(key, 0, sizeof(key));
                    memset(value, 0, sizeof(value));
                    // for MCUs without sscanf support, use library function
                    if (ecjp_split_key_and_value(item_list, key, value, ECJP_BOOL_FALSE) != ECJP_NO_ERROR) {
                        printf("Failed to split key-value pair.\n");
                    } else  {
                        printf("Key = '%s', Value = '%s'\n", key, value);
                        if (api_parse_keys[api_index].parse_func != NULL) {
                            res = api_parse_keys[api_index].parse_func(value);
                            if (res != WLT_SUCCESS) {
                                printf("Failed to parse value for key '%s'\n", key);
                                res = WLT_GENERIC_ERROR;
                            } else {
                                printf("Parsed value for key '%s' successfully.\n", key);
                            }
                        } else {
                            printf("No parse function defined for key '%s'\n", key);
                            res = WLT_GENERIC_ERROR;
                        }
                    }
                    break;
                
                default:
                    printf("Unknown type.\n");
                    break;
            }
            item_list = item_list->next;
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}

/**
 * Function: parse_post_body()
 * Description: This function parses the settings form data received from the client.
 * Parameters:
 * body - pointer to the body of the HTTP request
 * content_length - length of the content
 */
wlt_error_t parse_post_body(char *body, size_t content_length)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;

    printf("Len = %d - %s\n", content_length, body);

    res = WLT_GENERIC_ERROR;

    // Parse the form data using ecjp
    ret = ecjp_check_and_load_2(body, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing settings form data: %d\n", ret);
        ecjp_free_item_list(&item_list);
        return WLT_GENERIC_ERROR;
    }
    else {
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                case ECJP_TYPE_ARRAY:
                    printf("Parsing Object and Array not supported\n");
                    break;
                
                case ECJP_TYPE_KEY_VALUE_PAIR:
                    printf("Parsing key-value pair...\n");
                    memset(key, 0, sizeof(key));
                    memset(value, 0, sizeof(value));
                    // for MCUs without sscanf support, use library function
                    if (ecjp_split_key_and_value(item_list, key, value, ECJP_BOOL_FALSE) != ECJP_NO_ERROR) {
                        printf("Failed to split key-value pair.\n");
                    } else  {
                        printf("Key = '%s', Value = '%s'\n", key, value);
                        for (i=0; i<PARAMS_MAX; i++) {
                            if (strcmp(key, api_parse_keys[i].key) == 0) {
                                printf("Found key '%s', calling parse function...\n", key);
                                if (api_parse_keys[i].parse_func != NULL) {
                                    wlt_error_t parse_res = api_parse_keys[i].parse_func(value);
                                    if (parse_res != WLT_SUCCESS) {
                                        printf("Failed to parse value for key '%s'\n", key);
                                        res = WLT_GENERIC_ERROR;
                                    } else {
                                        printf("Parsed value for key '%s' successfully.\n", key);
                                    }
                                } else {
                                    printf("No parse function defined for key '%s'\n", key);
                                    res = WLT_GENERIC_ERROR;
                                }
                                break;
                            }
                        }
                        // todo: la funzione torna positivamente anche se trova solo una chiave valida.
                        // con /api/v1/setallparams bisogna che tutte le chiavi siano valide
                    }
                    break;
                
                default:
                    printf("Unknown type.\n");
                    break;
            }
            item_list = item_list->next;
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}
