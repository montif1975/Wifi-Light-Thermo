#ifndef UART_H
#define UART_H

// UART PARAMETERS
#define UART_ID                 uart0
#define BAUD_RATE               115200
#define DATA_BITS               8
#define STOP_BITS               1
#define PARITY                  UART_PARITY_NONE

// Use default UART pins 0 and 1
#define UART_TX_PIN             0
#define UART_RX_PIN             1

#endif // UART_H
