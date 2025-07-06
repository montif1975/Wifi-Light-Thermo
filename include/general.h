#ifndef GENERAL_H
#define GENERAL_H

#define DEBUG                               1
#define DEBUG_I2C

#ifdef DEBUG_GEN
#define PRINT_GEN_DEBUG(format, ...)   printf(format, __VA_ARGS__)
#else
#define PRINT_GEN_DEBUG(format, ...)
#endif

#ifdef DEBUG_I2C
#define PRINT_I2C_DEBUG(format, ...)   printf(format, __VA_ARGS__)
#else
#define PRINT_I2C_DEBUG(format, ...)
#endif

#define START_DNS_SERVER                    0
#endif // GENERAL_H
