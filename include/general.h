#ifndef GENERAL_H
#define GENERAL_H

#define DEBUG                               0
#define DEBUG_I2C

#ifdef DEBUG
#define PRINT_DEBUG_N(format)               printf(format)
#define PRINT_DEBUG(format, ...)            printf(format, __VA_ARGS__)
#else
#define PRINT_DEBUG_N(format)               printf(format)
#define PRINT_DEBUG(format, ...)
#endif

#ifdef DEBUG_I2C
#define PRINT_I2C_DEBUG(format, ...)        printf(format, __VA_ARGS__)
#else
#define PRINT_I2C_DEBUG(format, ...)
#endif

#define START_DNS_SERVER                    0

#define BYTE                                unsigned char
#endif // GENERAL_H
