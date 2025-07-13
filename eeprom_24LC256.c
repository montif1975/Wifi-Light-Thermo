#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "include/general.h"
#include "include/eeprom_24LC256.h"


// Initialise EEPROM interface
void i2c_eeprom_init(void)
{
    i2c_init(I2C_PORT, I2C_FREQ);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    
    return;
}

// Low level EEPROM Functions

// Function: i2c_eeprom_dump()
int i2c_eeprom_dump(void)
{
    int i, ret = 0;
    int npage = 0;
    int max_page = (EEPROM_MEM_LENGHT/EEPROM_PAGE_LEN);
    int ee_addr = EEPROM_START_ADDR;
    BYTE data[EEPROM_PAGE_LEN];

    for(npage = 0; npage < max_page; npage++)
    {
        memset(data,0,sizeof(data));
        ret = i2c_eeprom_read(ee_addr,data,EEPROM_PAGE_LEN);
        if(ret == EE_SUCCESS)
        {
            // print page
            printf("Page %d (start address = %02X):\n",npage,ee_addr);
            for(i=0; i<sizeof(data); i++)
            {
                printf("0x%02X ",data[i]);
                if((i + 1) % (EEPROM_PAGE_LEN/2) == 0)
                    printf("\n");
            }
            ee_addr += EEPROM_PAGE_LEN;
        }
        else
            printf("Fail to read page %d\n", npage);
    }

    return ret;
}

// Read from EEPROM
int i2c_eeprom_read(int addr, BYTE *data, int dlen)
{
    int ret;
    BYTE cmd[2] = {(BYTE)(addr >> 8), (BYTE)addr};
        
    ret = i2c_write_timeout_us(I2C_PORT, EEPROM_ADDR, cmd, sizeof(cmd), 1, EEPROM_XFER_US);
    if ((ret != PICO_ERROR_GENERIC) && (ret != PICO_ERROR_TIMEOUT))
    {
        ret = i2c_read_timeout_us(I2C_PORT, EEPROM_ADDR, data, dlen, 0, EEPROM_XFER_US);
        if (ret > 0)
            ret = EE_SUCCESS;
    }
    if (ret != EE_SUCCESS)
    {
        PRINT_DEBUG("%s: Error: %d\n",__FUNCTION__,ret);
        ret = EE_ERROR;
    }

    return ret;
}
    
// Write to EEPROM
int i2c_eeprom_write(int addr, BYTE *data, int dlen)
{
    BYTE cmd[EEPROM_PAGE_LEN+2];
    int n;
    int i = 0;
    int a = (addr & (EEPROM_PAGE_LEN - 1));
    int ret = EE_SUCCESS;
    
    while (dlen > 0)
    {
        n = a + dlen > EEPROM_PAGE_LEN ? EEPROM_PAGE_LEN - a : dlen;
        cmd[0] = (BYTE)((addr + i) >> 8);
        cmd[1] = (BYTE)(addr + i);
        memcpy(&cmd[2], &data[i], n);
        ret = i2c_write_timeout_us(I2C_PORT, EEPROM_ADDR, cmd, n + 2, 0, EEPROM_XFER_US);
        if((ret == PICO_ERROR_GENERIC) || (ret == PICO_ERROR_TIMEOUT))
        {
            PRINT_DEBUG("%s: Write only %d bytes, Error: %d",__FUNCTION__, i, ret);
            ret = EE_ERROR;
            break;
        }
        else
            ret = EE_SUCCESS;
        i += n;
        dlen -= n;
        a = 0;
        busy_wait_us(EEPROM_WRITE_US);
    }
    return ret;
}

