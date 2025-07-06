#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "general.h"
#include "include/dht20.h"

/*
 * Function: C2F(temperature)
 * Convert Celsius to Fahrenheit scale
*/
float C2F(float temperature) {
    return temperature * (9.0f / 5) + 32;
}

/*
 * Function: DHT20_init()
 * Check connection and verifiy if the sensor is inizialized
 */
int DHT20_init(void)
{
    int ret = 0;
    uint8_t buf[6] = {DHT20_READ,0,0,0,0,0};

    i2c_write_blocking(I2C_PORT_SENS,DHT20_I2C_ADDRESS,buf,1,false);
    memset(buf,0x0,sizeof(buf));
    sleep_ms(100);
    i2c_read_blocking(I2C_PORT_SENS,DHT20_I2C_ADDRESS,buf,1,false);
    printf("--> from sensor: buf[0] = %02x\n",buf[0]);
    if(buf[0] == DHT20_STATUS_OK)
    {
        printf("Sensor DHT20 calibrated and ready.\n");
    }
    else
    {
        printf("Sensor DHT20 NOT calibrated or not found!\n");
        ret = -1;
    }
    return ret;
}

/*
 * Function: DHT20_read_data()
 * Start measure procedere and ready the reply when the device is ready 
 */
int DHT20_read_data(float *temp, float *hum)
{
    int ret = 0;
    int temperature = 0;
    int humidity = 0;
    uint8_t buf[6] = {DHT20_READ,0,0,0,0,0};
    uint8_t loop;

    // start measure
    memset(buf,0x0,sizeof(buf));
    buf[0] = DHT20_START_MEASURE;
    buf[1] = 0x33;
    buf[2] = 0x00;
    i2c_write_blocking(I2C_PORT_SENS,DHT20_I2C_ADDRESS,buf,3,false);
    
    // read the response waiting until it's ready
    // TODO: modificare e leggere direttamente 6 bytes con il primo che e' lo status
    loop = DTH20_WAIT_MEAS_LOOP;
    do {
        memset(buf,0x0,sizeof(buf));
        sleep_ms(DHT20_WAIT_MEAS_MS);
        i2c_read_blocking(I2C_PORT_SENS,DHT20_I2C_ADDRESS,buf,6,false);
        PRINT_I2C_DEBUG("--> loop = %d, from sensor: buf[0] = %02x\n",loop,buf[0]);
        loop--;
    } while (((buf[0] & 0x80) == 0x80) && (loop > 0));
    if(loop == 0)
    {
        PRINT_I2C_DEBUG("Unable to read measure within %d milleseconds\n",(DHT20_WAIT_MEAS_MS*DTH20_WAIT_MEAS_LOOP));
        ret = -1;
        return ret;
    }
    
    // measure is ready
    i2c_read_blocking(I2C_PORT_SENS,DHT20_I2C_ADDRESS,buf,6,false);
    PRINT_I2C_DEBUG("--> from sensor: buf[] = %02x,%02x,%02x,%02x,%02x\n",buf[1],buf[2],buf[3],buf[4],buf[5]);
    humidity = (buf[1]<<8) | ((buf[2]));
    humidity = ((humidity << 4) | ((buf[3] & 0xF0)>>4));
    PRINT_I2C_DEBUG("humidity = 0x%X\n",humidity);
    temperature = ((buf[3] & 0x0F)<< 16) | (buf[4]<<8) | buf[5];
    PRINT_I2C_DEBUG("temperature = 0x%X\n",temperature);
    *hum = ((float)humidity / 1048576)*100;
    *temp = (((float)temperature / 1048576)*200)-50;
    PRINT_I2C_DEBUG("*** TEMPERATURE = %.2f C\n",*temp);
    PRINT_I2C_DEBUG("*** HUMIDITY = %.2f %%RH\n",*hum);

    return ret;
}
