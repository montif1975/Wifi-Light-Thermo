#ifndef EEPROM_24LC256_H
#define EEPROM_24LC256_H

#define EE_SUCCESS              0
#define EE_ERROR                -1

// I2C defines
// This example will use I2C0 on GPIO14 (SDA) and GPIO15 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT                i2c1
#define I2C_SDA                 14
#define I2C_SCL                 15
#define I2C_FREQ                (400 * 1000)

// Control code = 0x50
// Chip Select bit = A2, A1, A0.
// in this implementation: A2=1, A1=0, A0=0
// therefore the address is 1010100 = 0x54 
#define EEPROM_ADDR             0x54
#define EEPROM_PAGE_LEN         64
#define EEPROM_WRITE_US         11000
#define EEPROM_XFER_US          100000

#define EEPROM_MEM_LENGHT       0x8000
#define EEPROM_START_ADDR       0x0
#define EEPROM_STOP_ADDR        (EEPROM_MEM_LENGHT - 1)

#define EEPROM_CTRL_WORD        "WifiLightThermo\0"
#define EEPROM_CTRL_WORD_LEN    16

// Function prototypes
void i2c_eeprom_init(void);
int i2c_eeprom_dump(void);
int i2c_eeprom_read(int addr, BYTE *data, int dlen);
int i2c_eeprom_write(int addr, BYTE *data, int dlen);

#endif // EEPROM_24LC256_H 
