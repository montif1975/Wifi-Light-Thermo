# WiFi Light Thermo (wlt)

NOTE: This project is based on the Raspberry PICO example [tcp_server] (https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/tcp_server/).

WiFi Light Thermo, implements a WiFi-enabled thermostat that measures temperature and humidity. The measured values can be accessed via a web server interface—either through a simple formatted web page, an API endpoint, or a serial log.

The device allows configuration of several parameters, including:

- Temperature unit: °C or °F

- Output format: TXT or CSV

- Sensor polling interval

- Temperature and humidity thresholds to activate or deactivate predefined outputs

You can also set the SSID and password to connect the device to a desired WiFi network.
All configuration settings are stored in EEPROM memory connected via the I2C bus.

The device's WiFi mode—Access Point (AP) or Station (STA)—is selected based on the state of a predefined GPIO pin (default: GPIO 22).

Sensor readings are always available, but configuration is only possible when the device is operating in Access Point mode.

## Hardware configuration

fff

## Connections

kk

## How to works

ff

## Web interface

dd

## Serial interface

ff


