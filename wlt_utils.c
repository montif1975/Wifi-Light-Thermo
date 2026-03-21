#include "include/wlt.h"
#include "include/wlt_global.h"

/*
 * Function: C2F(temperature)
 * Convert Celsius to Fahrenheit scale
*/
float C2F(float temperature) {
    return temperature * (9.0f / 5) + 32;
}

/*
 * Function: check_wifi_password()
 * Description: This function checks if the provided Wi-Fi password is valid.
 * It returns 1 if the password is valid, 0 if it's not valid, -1 if it's not change.
 */
int check_wifi_password(const char *password)
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
void fix_devname(const char *src, size_t src_len, char *dest, size_t dest_len)
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
