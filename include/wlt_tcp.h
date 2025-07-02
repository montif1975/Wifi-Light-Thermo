#include "stdbool.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define TCP_PORT                            80
#define POLL_TIME_S                         5
#define HTTP_GET                            "GET"
#define HTTP_POST                           "POST"
#define HTTP_RESPONSE_HEADERS               "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/%s; charset=utf-8\nConnection: close\n\n"

//#define INFO_BODY                           "<html><body><h1>Hello from My Pico.</h1><p>Wifi SSID: %s</p><p>Wifi Password: %s</p><p>IP Address: %s</p><p>IP Mask: %s</p><p>IP Gateway: %s</p></body></html>"
//#define INFO_BODY_AP                        "<html><body><h1>Hello from My Pico.</h1><p>Wifi SSID: %s</p><p>Wifi Password: %s</p></body></html>"
//#define INFO_BODY_REQ                       "<body><h1>WiFi Sensors</h1><p>Temperature: %.02f</p><p>Humidity: %.02f</p></body>"

#define STYLE_CSS                           "body {\
font-family: Arial, sans-serif;\
background-color: #f4f6f8;\
color: #333;\
display: flex;\
justify-content: center;\
align-items: flex-start;\
min-height: 100vh;\
padding-top: 50px;\
margin: 0;\
}\
.form-container {\
background-color: #ffffff;\
padding: 30px;\
border-radius: 8px;\
box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);\
width: 300px;\
}\
h2 {\
color: #2c3e50;\
margin-bottom: 20px;\
text-align: center;\
}\
label {\
display: block;\
margin-bottom: 6px;\
font-weight: 500;\
color: #34495e;\
}\
input[type=\"text\"],\
input[type=\"password\"] {\
width: 100%%;\
padding: 8px;\
margin-bottom: 16px;\
border: 1px solid #ccd6dd;\
border-radius: 4px;\
font-size: 14px;\
transition: border-color 0.3s ease;\
}\
input[type=\"text\"]:focus {\
border-color: #3498db;\
outline: none;\
}\
input[type=\"submit\"] {\
width: 100%%;\
padding: 10px;\
background-color: #3498db;\
color: white;\
border: none;\
border-radius: 4px;\
font-size: 16px;\
cursor: pointer;\
transition: background-color 0.3s ease;\
}\
input[type=\"submit\"]:hover {\
background-color: #2980b9;\
}\r\n"

#define STYLE_INFO_CSS                        "body {\
font-family: Arial, sans-serif;\
background-color: #f4f6f8;\
color: #333;\
display: flex;\
justify-content: center;\
align-items: flex-start;\
min-height: 100vh;\
padding-top: 50px;\
margin: 0;\
}\n\
.info-container {\
background-color: #ffffff;\
padding: 30px;\
border-radius: 8px;\
box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);\
width: 400px;\
font-size: 2rem;\
}\n\
h2 {\
color: #2c3e50;\
margin-bottom: 20px;\
text-align: center;\
font-size: 1.5rem;\
}\n\
h3 {\
color:rgb(90, 100, 240);\
margin-bottom: 20px;\
text-align: center;\
}\n\
.info-data {\
display: flex;\
flex-direction: column;\
gap: 10px;\
align-items: stretch;\
}\n\
.info-data label span.value {\
font-family: 'Consolas', 'Courier New', monospace;\
font-size: 2.2rem;\
color: rgb(0, 0, 255);\
font-weight: bold;\
}\r\n"

#define INFO_REPLY_HEAD                     "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Sensor info</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style_info.css\">\
</head>"

#define INFO_REPLY_BODY                     "<body>\
<div class=\"info-container\">\
<h2>WiFi Sensor</h2>\
<div class=\"info-data\">\
<label><span class=\"value\">%.02f %s</span></label>\
<label><span class=\"value\">%.02f %%RH</span></label>\
</div>\
</div>\
</body>\
</html>\r\n"

#define SETTINGS_REPLY_HEAD                 "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Config Form</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Wifi Sensor Configuration</h2>\
<form action=\"/setparams\" method=\"get\">\
<h3>Wifi config</h3>\
<hr>"

#define SETTINGS_REPLY_FORM_WIFI            "<label for=\"ssid\">SSID</label>\
<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"%s\">\
<label for=\"pwd\">Password</label>\
<input type=\"password\" id=\"pwd\" name=\"pwd\" value=\"***\">\
<h3>Sensor config</h3>\
<hr>\
<div class=\"sc\">"

#define SETTINGS_REPLY_FORM_SENSOR          "<label>Temperature displayed in:</label>\
<label><input type=\"radio\" name=\"scale\" value=\"C\" %s>&degC</label>\
<label><input type=\"radio\" name=\"scale\" value=\"F\" %s>&degF</label>\
<label>Output format:</label>\
<label><input type=\"radio\" name=\"oform\" value=\"TXT\" %s>TXT</label>\
<label><input type=\"radio\" name=\"oform\" value=\"CSV\" %s>CSV</label>"

#define SETTINGS_REPLY_FOOTER               "</div>\
<hr>\
<input type=\"submit\" value=\"Save\">\
</form>\
<h2>Advanced Settings</h2>\
<label for=\"advanced\">Advanced settings are available in the <a href=\"/advanceparams\">Advanced Settings</a> page.</label>\
</div>\
</body>\
</html>\r\n"

#define SETTINGS_REPLY_NACK                "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Config Form</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Sorry, but the configuration page is available only in AP mode</h2>\
<h3>Please switch your device in AP mode and reboot it to configure.</h3>\
</div>\
</body>\
</html>\r\n"

#define SETTINGS_SAVE_ACK                   "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Configuration saved</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Settings saved!</h2>\
<h3><a href=\"/info\">Click here to access sensor's data</a></h3>\
</div>\
</body>\
</html>\r\n"

#define SETTINGS_SAVE_NACK_EINVAL           "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Configuration saved</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Invalid Parameters!</h2>\
<h3><a href=\"/settings\">Click here to configure</a></h3>\
</div>\
</body>\
</html>\r\n"

#define SETTINGS_SAVE_NACK_ENOPARAM         "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Configuration saved</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>No Parameters found!</h2>\
<h3><a href=\"/settings\">Click here to configure</a></h3>\
</div>\
</body>\
</html>\r\n"


#define API_INFO_REPLY                      "{\n\"T\":\"%.2f\",\n\"TF\":\"%s\",\n\"H\":\"%.2f\"\n}"

#define HTTP_RESPONSE_REDIRECT              "HTTP/1.1 302 Redirect\nLocation: http://%s" INFO_URL "\n\n"
#define HTTP_RESPONSE_NOT_FOUND             "HTTP/1.1 404 Not Found\nContent-Length: 0\nConnection: close\n\n"
#define HTTP_RESPONSE_INTERNAL_ERROR        "HTTP/1.1 500 Internal Server Error\nContent-Length: 0\nConnection: close\n\n"
#define HTTP_RESPONSE_NOT_IMPL_ERROR        "HTTP/1.1 501 Not implemented\nContent-Length: 0\nConnection: close\n\n"

#define INFO_URL                            "/info"
#define STYLE_URL                           "/style.css"
#define STYLE_INFO_URL                      "/style_info.css"
#define SETTINGS_URL                        "/settings"
#define SETTINGS_FORM_URL                   "/setparams"
#define ADVANCED_URL                        "/advanceparams"
#define ADVANCED_FORM_URL                   "/setadvanceparams" 

#define API_BASE_URL                        "/api"
#define API_VERS                            "/v1"
#define API_GET_INFO_URL                    API_BASE_URL API_VERS "/info"
#define API_SET_PARAMS_URL                  API_BASE_URL API_VERS "/setparams"
#define API_SET_ADVANCED_PARAMS_URL         API_BASE_URL API_VERS "/setadvanceparams"

enum http_req_page {
    HTTP_REQ_STYLE,
    HTTP_REQ_STYLE_INFO,
    HTTP_REQ_INFO,
    HTTP_REQ_SETTINGS,
    HTTP_REQ_SETTINGS_FORM,
    HTTP_REQ_ADVANCED,
    HTTP_REQ_ADVANCED_FORM,
    HTTP_REQ_MAX
};

enum http_req_api {
    HTTP_API_INFO,
    HTTP_API_SET_PARAMS,
    HTTP_API_SET_ADVANCED_PARAMS,
    HTTP_API_MAX   
};

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[128];
    char result[1152];
    int header_len;
    int result_len;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;

bool tcp_server_open(void *arg, const char *ap_name);
void tcp_server_close(TCP_SERVER_T *state);