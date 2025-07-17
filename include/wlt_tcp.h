#ifndef WLT_TCP_H
#define WLT_TCP_H

#include "stdbool.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define TCP_PORT                            80
#define POLL_TIME_S                         5
#define HTTP_GET                            "GET"
#define HTTP_POST                           "POST"
#define HTTP_RESPONSE_HEADERS               "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/%s; charset=utf-8\nConnection: close\r\n\r\n"
#define HTTP_RESPONSE_HEADERS_IMAGE         "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: image/%s\nConnection: close\r\n\r\n"
#define HTTP_RESPONSE_HEADERS_JSON          "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: application/%s\nConnection: close\r\n\r\n"

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
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\
<title>Sensor info</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style_info.css\">\
<script>\
setInterval(() => { location.reload(); }, 10000);\
</script>\
</head>"

#define INFO_REPLY_BODY                     "<body>\
<div class=\"info-container\">\
<h2>%s</h2>\
<div class=\"info-data\">\
<label><span class=\"value\">%.02f %s</span></label>\
<label><span class=\"value\">%.02f %%RH</span></label>\
</div>\
<h2 id=\"datetime\"></h2>\
<script>\
document.getElementById('datetime').textContent = new Date().toLocaleString();\
</script>\
</div>\
</body>\
</html>"

#define INFO_REPLAY_BODY_NOT_VALID         "<body>\
<div class=\"info-container\">\
<h2>%s</h2>\
<div class=\"info-data\">\
<p>Sensor not available or last data read is not valid!</p>\
</div>\
</div>\
</body>\
</html>"

// Settings page reply
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
<h3>Wifi config (STA mode)</h3>\
<hr>"

#define SETTINGS_REPLY_FORM_WIFI            "<label for=\"ssid\">SSID</label>\
<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"%s\">\
<label for=\"pwd\">Password</label>\
<input type=\"password\" id=\"pwd\" name=\"pwd\" value=\"%s\">\
<label for=\"devname\">Device Name</label>\
<input type=\"text\" id=\"devname\" name=\"devname\" value=\"%s\">\
<h3>Sensor config</h3>\
<hr>\
<div class=\"sc\">"

#define SETTINGS_REPLY_FORM_SENSOR          "<p>Temperature displayed in:</p>\
<label><input type=\"radio\" name=\"scale\" value=\"C\" %s>&degC</label>\
<label><input type=\"radio\" name=\"scale\" value=\"F\" %s>&degF</label>\
<p>Output format:</p>\
<label><input type=\"radio\" name=\"oform\" value=\"TXT\" %s>TXT</label>\
<label><input type=\"radio\" name=\"oform\" value=\"CSV\" %s>CSV</label>"

#define SETTINGS_REPLY_FOOTER               "</div>\
<hr>\
<input type=\"submit\" value=\"Save\">\
</form>\
<h2>Advanced Settings</h2>\
<p>Advanced settings are available <a href=\"/advparams\">here</a></p>\
</div>\
</body>\
</html>"

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
</html>"

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
</html>"

#define SETTINGS_SAVE_NACK_EINVAL           "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Configuration error</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Invalid Parameters!</h2>\
<h3><a href=\"/settings\">Click here to configure</a></h3>\
</div>\
</body>\
</html>"

#define SETTINGS_SAVE_NACK_ENOPARAM         "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Configuration error</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>No Parameters found!</h2>\
<h3><a href=\"/settings\">Click here to configure</a></h3>\
</div>\
</body>\
</html>"

// settings advanced reply
#define ADVANCED_REPLY_HEAD         "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Advance Config Form</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Advanced configuration</h2>\
<form action=\"/setadvparams\" method=\"get\">\
<h3>Sensor config</h3>\
<hr>"

#define ADVANCED_REPLY_FORM_SENSOR  "<div class=\"input-group\">\
<p>Polling timer [s]</p>\
<input type=\"number\" id=\"ptime\" name=\"ptime\" min=\"%d\" max=\"%d\" step=\"1\" value=\"%d\">\
</div>"

// I need to change the form to use links for setting thresholds
// due the limitation in the lenght of the TCP message in the web server implementation using lwIP
#define ADVANCED_REPLY_FORM_THRESHOLDS  "<div class=\"input-group\">\
<h3>Threshold config</h3>\
<hr>\
<p>Set high temperature threshold <a href=\"/sethightemp\">here</a></p>\
<p>Set low temperature threshold <a href=\"/setlowtemp\">here</a></p>\
<hr>\
<p>Set high humidity threshold <a href=\"/sethighhum\">here</a></p>\
<p>Set low humidity threshold <a href=\"/setlowhum\">here</a></p>\
<hr>\
</div>"

#define ADVANCED_REPLY_FOOTER               "<input type=\"submit\" value=\"Save\">\
</form>\
<div>\
<p>Return to <a href=\"/settings\">Settings</a></p>\
</div>\
</div>\
</body>\
</html>"

#define ADVANCED_SAVE_ACK                   "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Advanced configuration saved</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Advance settings saved!</h2>\
<h3><a href=\"/info\">Click here to access sensor's data</a></h3>\
</div>\
</body>\
</html>"

#define ADVANCED_SAVE_NACK_EINVAL           "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Advanced configuration error</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>Invalid Parameters!</h2>\
<h3><a href=\"/advparams\">Click here to configure</a></h3>\
</div>\
</body>\
</html>"

#define ADVANCED_SAVE_NACK_ENOPARAM         "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Advanced configuration error</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<div class=\"form-container\">\
<h2>No Parameters found!</h2>\
<h3><a href=\"/advparams\">Click here to configure</a></h3>\
</div>\
</body>\
</html>"

#define REPLY_NOT_YET_IMPLEMENTED           "<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
<meta charset=\"UTF-8\">\
<title>Advice</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\
</head>\
<body>\
<h2>Sorry, this page is not implemented yet!</h2>\
</body>\
</html>"

#define API_INFO_REPLY                      "{\"T\":%.2f,\"TF\":\"%s\",\"H\":%.2f}"

#define HTTP_RESPONSE_REDIRECT              "HTTP/1.1 302 Redirect\nLocation: http://%s" INFO_URL "\r\n\r\n"
#define HTTP_RESPONSE_NOT_FOUND             "HTTP/1.1 404 Not Found\nContent-Length: 0\nConnection: close\r\n\r\n"
#define HTTP_RESPONSE_INTERNAL_ERROR        "HTTP/1.1 500 Internal Server Error\nContent-Length: 0\nConnection: close\r\n\r\n"
#define HTTP_RESPONSE_NOT_IMPL_ERROR        "HTTP/1.1 501 Not implemented\nContent-Length: 0\nConnection: close\r\n\r\n"

#define STYLE_URL                           "/style.css"
#define STYLE_INFO_URL                      "/style_info.css"
#define FAVICON_URL                         "/favicon.ico"
#define INFO_URL                            "/info"
#define SETTINGS_URL                        "/settings"
#define SETTINGS_FORM_URL                   "/setparams"
#define ADVANCED_URL                        "/advparams"
#define ADVANCED_FORM_URL                   "/setadvparams"
#define SET_HIGH_TEMP_URL                   "/sethightemp"
#define SET_HIGH_TEMP_FORM_URL              "/sethightempform"
#define SET_LOW_TEMP_URL                    "/setlowtemp"
#define SET_LOW_TEMP_FORM_URL               "/setlowtempform"
#define SET_HIGH_HUM_URL                    "/sethighhum"
#define SET_HIGH_HUM_FORM_URL               "/sethighhumform"
#define SET_LOW_HUM_URL                     "/setlowhum"
#define SET_LOW_HUM_FORM_URL                "/setlowhumform"


#define API_BASE_URL                        "/api"
#define API_VERS                            "/v1"
#define API_GET_INFO_URL                    API_BASE_URL API_VERS "/info"
#define API_GET_SETTINGS_URL                API_BASE_URL API_VERS "/settings"
#define API_SET_PARAMS_URL                  API_BASE_URL API_VERS "/setparams"
#define API_SET_ADVANCED_PARAMS_URL         API_BASE_URL API_VERS "/setadvparams"
#define API_SET_HIGH_TEMP_URL               API_BASE_URL API_VERS "/sethightemp"
#define API_SET_LOW_TEMP_URL                API_BASE_URL API_VERS "/setlowtemp"
#define API_SET_HIGH_HUM_URL                API_BASE_URL API_VERS "/sethighhum"
#define API_SET_LOW_HUM_URL                 API_BASE_URL API_VERS "/setlowhum"


enum http_req_page {
    HTTP_REQ_STYLE,
    HTTP_REQ_STYLE_INFO,
    HTTP_REQ_FAVICON,
    HTTP_REQ_INFO,
    HTTP_REQ_SETTINGS,
    HTTP_REQ_SETTINGS_FORM,
    HTTP_REQ_ADVANCED,
    HTTP_REQ_ADVANCED_FORM,
    HTTP_REQ_SET_HIGH_TEMP,
    HTTP_REQ_SET_HIGH_TEMP_FORM,
    HTTP_REQ_SET_LOW_TEMP,
    HTTP_REQ_SET_LOW_TEMP_FORM,
    HTTP_REQ_SET_HIGH_HUM,
    HTTP_REQ_SET_HIGH_HUM_FORM,
    HTTP_REQ_SET_LOW_HUM,
    HTTP_REQ_SET_LOW_HUM_FORM,
    HTTP_REQ_MAX
};

enum http_req_api {
    HTTP_API_INFO,
    HTTP_API_GET_SETTINGS,
    HTTP_API_SET_PARAMS,
    HTTP_API_SET_ADVANCED_PARAMS,
    HTTP_API_SET_HIGH_TEMP,
    HTTP_API_SET_LOW_TEMP,
    HTTP_API_SET_HIGH_HUM,
    HTTP_API_SET_LOW_HUM,
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

#endif // WLT_TCP_H
