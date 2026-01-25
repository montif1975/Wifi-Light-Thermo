#include "include/wlt.h"
#include "include/wlt_global.h"
#include "json/ecjp.h"

// forward declarations of API parse functions
wlt_error_t api_parse_wifi(char *wifi_params);
wlt_error_t api_parse_settings(char *settings_params);
wlt_error_t api_parse_thresholds(char *thresholds_params);

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
        ecjp_free_item_list(&item_list);
        return WLT_GENERIC_ERROR;
    }
    else {
        printf("WIFI data parsed successfully\n");
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Item read successfully.\n");
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                case ECJP_TYPE_ARRAY:
                    printf("Item Object and Array not supported.\n");
                    res = WLT_GENERIC_ERROR;
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
                        for (i=0; i<WIFI_PARAM_MAX; i++) {
                            if (strcmp(key, api_wifi_params[i]) == 0) {
                                printf("Found key '%s', calling parse function...\n", key);
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
        }
    }
    ecjp_free_item_list(&item_list);
    return res;
}

wlt_error_t api_parse_settings(char *settings_params)
{
    // Dummy implementation
    return WLT_SUCCESS;
}

wlt_error_t api_parse_thresholds(char *thresholds_params)
{
    // Dummy implementation
    return WLT_SUCCESS;
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
        printf("Settings form data parsed successfully\n");
        res = WLT_SUCCESS;
        while (item_list != NULL) {
            printf("Item read successfully.\n");
            printf("Type = %s, Value = %s\n", ecjp_type[item_list->item.type], (char *)item_list->item.value);
            switch (item_list->item.type) {
                case ECJP_TYPE_OBJECT:
                    printf("Parsing object...\n");
//                  parse_level_2((char *)item_list->item.value);
                    break;
                
                case ECJP_TYPE_ARRAY:
                    printf("Parsing array...\n");
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
