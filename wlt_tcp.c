#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "include/wlt.h"
#include "include/wlt_global.h"
//#include "include/wlt_tcp.h"

static char *http_req_page_str[HTTP_REQ_MAX] = {
    STYLE_URL,
    STYLE_INFO_URL,
    INFO_URL,
    SETTINGS_URL
};

static char *http_req_api_str[HTTP_API_MAX] = {
    API_GET_INFO_URL,
    API_SET_PARAMS_URL
};


static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err) {
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

void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_sent %u\n", len);
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        printf("all done\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

static int test_server_content(const char *request, const char *params, char *result, size_t max_result_len) {
    int len = 0;
    int len2copy = 0;
    int i = 0;
#if DEBUG
    printf("%s Request: %s?%s\n", __FUNCTION__, request, params);
    printf("max_result_len: %zu\n", max_result_len);
#endif

    if(pconfig == NULL) {
        printf("pconfig is NULL\n");
        return 0; // Error
    }
#if DEBUG
    else {
        printf("pconfig is not NULL\n");
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

            case HTTP_REQ_INFO:
                // first copy the head section and then the body
                len2copy = strlen(INFO_REPLY_HEAD);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small for info head (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, INFO_REPLY_HEAD);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
                // check if we have enough space for the body
                len2copy = strlen(INFO_REPLY_BODY);
                if (len + len2copy >= max_result_len) {
                    printf("Result buffer too small for info content body (len=%d, len2copy=%d, max_result_len=%zu)\n", len, len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, INFO_REPLY_BODY,
                        pconfig->data.temperature,
                        pconfig->data.humidity);
                }      
                break;

            case HTTP_REQ_SETTINGS:
#if DEBUG
                // copy the settings form
                len2copy = strlen(SETTINGS_REPLY);
                if (len2copy >= max_result_len) {
                    printf("Result buffer too small (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                    return 0; // Error
                }
                else {
                    len += snprintf(result + len, max_result_len - len, SETTINGS_REPLY, pconfig->net_config.wifi_ssid);
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
#else
                // this page is available only when in AP mode
                if (pconfig->net_config.wifi_mode != WLT_WIFI_MODE_AP) {
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
                    len2copy = strlen(SETTINGS_REPLY);
                    if (len2copy >= max_result_len) {
                        printf("Result buffer too small (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                        return 0; // Error
                    }
                    else {
                        len += snprintf(result + len, max_result_len - len, SETTINGS_REPLY, pconfig->net_config.wifi_ssid);
                    }
                }
                if (len < 0) {
                    printf("Error generating info content\n");
                    return 0; // Error
                }
#endif
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
                                        pconfig->data.temperature,
                                        pconfig->data.settings.options.t_format == T_FORMAT_CELSIUS ? "C" : "F",
                                        pconfig->data.humidity);
                    if (len2copy >= max_result_len) {
                        printf("Result buffer too small for API info (len2copy=%d, max_result_len=%zu)\n", len2copy, max_result_len);
                        return 0; // Error
                    }
                    len = len2copy;
                    break;

                case HTTP_API_SET_PARAMS:
                    // Handle setting parameters (not implemented)
                    len = snprintf(result, max_result_len, "{\"status\":\"not_implemented\"}");
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

err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
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
            con_state->result_len = test_server_content(request, params, con_state->result, sizeof(con_state->result));

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

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_poll_fn\n");
    return tcp_close_client_connection(con_state, pcb, ERR_OK); // Just disconnect clent?
}

static void tcp_server_err(void *arg, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (err != ERR_ABRT) {
        printf("tcp_client_err_fn %d\n", err);
        tcp_close_client_connection(con_state, con_state->pcb, err);
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
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

bool tcp_server_open(void *arg, const char *ap_name) {
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

