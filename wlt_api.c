#include "include/wlt.h"
#include "include/wlt_global.h"
#include "json/ecjp.h"

// forward declarations of API parse functions
int check_and_remove_quotes(char *str);
wlt_error_t api_parse_wifi(char *wifi_params);
wlt_error_t api_parse_settings(char *settings_params);
wlt_error_t api_parse_thresholds(char *thresholds_params);
wlt_error_t api_parse_threshold_subparam(int subparam_index,char *value);

api_parse_key_t api_parse_keys[PARAMS_MAX] = {
    {"WIFI", api_parse_wifi},
    {"SETTINGS", api_parse_settings},
    {"THRESH", api_parse_thresholds}
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
    "TH"
};

static char *api_thresholds_params[THRESHOLDS_MAX] = {
    "HTT",
    "HTH",
    "LTT",
    "LTH"
};

static char *threshold_subparams[THRESH_SUBPARAM_MAX] = {
    "VAL",
    "TR"
};

static char *threshold_trigger_types[TRD_TRIGGER_MAX] = {
    "NONE",
    "H",
    "L",
    "B"
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
                                printf("Found key '%s', parsing...\n", key);
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
 * Function: api_parse_thresholds()
 * Description: This function parses the THRESHOLD parameters from a JSON string.
 * Parameters:
 * thresholds_params - pointer to the JSON string containing THRESHOLD parameters
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_thresholds(char *thresholds_params)
{
    wlt_error_t res;
    ecjp_return_code_t ret;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char value[ECJP_MAX_KEY_VALUE_LEN];
    int i;
    
    printf("Parsing THRESHOLD parameters...\n");
    ret = ecjp_check_and_load_2(thresholds_params, &item_list, &results);
    if (ret != ECJP_NO_ERROR) {
        printf("Error parsing thresholds form data: %d\n", ret);
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
                        for (i=0; i<THRESHOLDS_MAX && res == WLT_SUCCESS; i++) {
                            if (strcmp(key, api_thresholds_params[i]) == 0) {
                                printf("Found key '%s', parsing...\n", key);
                                switch (i) {
                                    case THRESHOLDS_HIGH_TEMP:
                                    case THRESHOLDS_HIGH_HUM:
                                    case THRESHOLDS_LOW_TEMP:
                                    case THRESHOLDS_LOW_HUM:
                                        {
                                            // value is expected to be a JSON object with subparameters
                                            printf("Parsing threshold subparameters for '%s'\n", key);
                                            wlt_error_t subparam_res = api_parse_threshold_subparam(i, value);
                                            if (subparam_res != WLT_SUCCESS) {
                                                printf("Failed to parse threshold subparameters for '%s'\n", key);
                                                res = WLT_GENERIC_ERROR;
                                            } else {
                                                printf("Parsed threshold subparameters for '%s' successfully.\n", key);
                                            }
                                        }
                                        break;

                                    default:
                                        printf("Unknown THRESHOLD parameter.\n");
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
 * Function: api_parse_threshold_subparam()
 * Description: This function parses a THRESHOLD subparameter from a JSON string.
 * Parameters:
 * subparam - index of subparameter name
 * value - pointer to the subparameter value
 * Returns:
 * WLT_SUCCESS on success, WLT_GENERIC_ERROR or WLT_INVALID_PARAM on failure
*/
wlt_error_t api_parse_threshold_subparam(int subparam_index,char *value)
{
    wlt_error_t res = WLT_SUCCESS;
    ecjp_check_result_t results;
    ecjp_item_elem_t *item_list = NULL;
    char key[ECJP_MAX_KEY_LEN];
    char val[ECJP_MAX_KEY_VALUE_LEN];
    int i;

    printf("Parsing threshold subparam '%s' with value '%s'\n", api_thresholds_params[subparam_index], value);
    // value is a JSON object with subparameters
    if (ecjp_check_and_load_2(value, &item_list, &results) != ECJP_NO_ERROR) {
        printf("Error parsing threshold subparam data\n");
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
                        for (i=0; i<THRESH_SUBPARAM_MAX && res == WLT_SUCCESS; i++) {
                            if (strcmp(key, threshold_subparams[i]) == 0) {
                                printf("Found key '%s', parsing...\n", key);
                                switch (subparam_index) {
                                    case THRESHOLDS_HIGH_TEMP:
                                        switch (i)
                                        {
                                            case THRESH_SUBPARAM_VALUE:
                                                {
                                                    float val = atof(value);
                                                    printf("Setting High Temperature Threshold to '%0.2f'\n", val);
                                                    prtconfig->data.thresholds.high.temperature.value = val;
                                                }
                                                break;

                                            case THRESH_SUBPARAM_TRIGGER:
                                                {
                                                    for (int j=0; j<TRD_TRIGGER_MAX; j++) {
                                                        if (strcmp(value, threshold_trigger_types[j]) == 0) {
                                                            printf("Setting High Temperature Threshold Trigger to '%s'\n", value);
                                                            prtconfig->data.thresholds.high.temperature.trigger = j;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;

                                            default:
                                                printf("Unknown THRESHOLD subparameter for param index %d.\n", subparam_index);
                                                res = WLT_GENERIC_ERROR;
                                                break;
                                        }
                                        break;

                                    case THRESHOLDS_HIGH_HUM:
                                        switch (i)
                                        {
                                            case THRESH_SUBPARAM_VALUE:
                                                {
                                                    float val = atof(value);
                                                    printf("Setting High Humidity Threshold to '%0.2f'\n", val);
                                                    prtconfig->data.thresholds.high.humidity.value = val;
                                                }
                                                break;

                                            case THRESH_SUBPARAM_TRIGGER:
                                                {
                                                    for (int j=0; j<TRD_TRIGGER_MAX; j++) {
                                                        if (strcmp(value, threshold_trigger_types[j]) == 0) {
                                                            printf("Setting High Humidity Threshold Trigger to '%s'\n", value);
                                                            prtconfig->data.thresholds.high.humidity.trigger = j;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;

                                            default:
                                                printf("Unknown THRESHOLD subparameter for param index %d.\n", subparam_index);
                                                res = WLT_GENERIC_ERROR;
                                                break;
                                        }
                                        break;

                                    case THRESHOLDS_LOW_TEMP:
                                        switch (i)
                                        {
                                            case THRESH_SUBPARAM_VALUE:
                                                {
                                                    float val = atof(value);
                                                    printf("Setting Low Temperature Threshold to '%0.2f'\n", val);
                                                    prtconfig->data.thresholds.low.temperature.value = val;
                                                }
                                                break;

                                            case THRESH_SUBPARAM_TRIGGER:
                                                {
                                                    for (int j=0; j<TRD_TRIGGER_MAX; j++) {
                                                        if (strcmp(value, threshold_trigger_types[j]) == 0) {
                                                            printf("Setting Low Temperature Threshold Trigger to '%s'\n", value);
                                                            prtconfig->data.thresholds.low.temperature.trigger = j;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;

                                            default:
                                                printf("Unknown THRESHOLD subparameter for param index %d.\n", subparam_index);
                                                res = WLT_GENERIC_ERROR;
                                                break;
                                        }
                                        break;

                                    case THRESHOLDS_LOW_HUM:
                                        switch (i)
                                        {
                                            case THRESH_SUBPARAM_VALUE:
                                                {
                                                    float val = atof(value);
                                                    printf("Setting Low Humidity Threshold to '%0.2f'\n", val);
                                                    prtconfig->data.thresholds.low.humidity.value = val;
                                                }
                                                break;

                                            case THRESH_SUBPARAM_TRIGGER:
                                                {
                                                    for (int j=0; j<TRD_TRIGGER_MAX; j++) {
                                                        if (strcmp(value, threshold_trigger_types[j]) == 0) {
                                                            printf("Setting Low Humidity Threshold Trigger to '%s'\n", value);
                                                            prtconfig->data.thresholds.low.humidity.trigger = j;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;

                                            default:
                                                printf("Unknown THRESHOLD subparameter for param index %d.\n", subparam_index);
                                                res = WLT_GENERIC_ERROR;
                                                break;
                                        }
                                        break;

                                    default:
                                        printf("Unknown THRESHOLD subparameter.\n");
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

/**
 * Function: parse_settings_form()
 * Description: This function parses the settings form data received from the client.
 * Parameters:
 * body - pointer to the body of the HTTP request
 * content_length - length of the content
 */
wlt_error_t parse_settings_form(char *body, size_t content_length)
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
