# WiFi Light Thermo (wlt)

NOTE: This project is based on the Raspberry PICO example [tcp_server](https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/tcp_server/).

WiFi Light Thermo, implements a WiFi-enabled thermostat that measures temperature and humidity. The measured values can be accessed via a web server interface—either through a simple formatted web page, an API endpoint, or a serial log.

The device allows configuration of several parameters, including:

- The device name

- Temperature unit: °C or °F

- Output format: TXT or CSV

- Sensor polling interval

- Temperature and humidity thresholds to activate or deactivate predefined outputs (not completely developed yet).

You can also set the SSID and password to connect the device to a desired WiFi network.
All configuration settings are stored in EEPROM memory connected via the I2C bus: I use a memory add-on available in my project [addon_eeprom_i2c](https://github.com/montif1975/addon_eeprom_i2c).

The device's WiFi mode — Access Point (AP) or Station (STA) — is selected based on the state of a predefined GPIO pin (default: GPIO 22).

To know the working status of the device, I added a RGB led that shows these colors according to the status:

| LED COLOR | Type   | Status of device |
|---------- |--------|------------------|
| RED       | SOLID  | BOOT IN PROGRESS |
| RED       | BLINK  | GENERIC ERROR    |
| GREEN     | BLINK  | RUN IN STA MODE  |
| BLUE      | BLINK  | RUN IN AP MODE   |
| YELLOW    | SOLID  | WIFI FAIL        |


Sensor readings are always available, but configuration is only possible when the device is operating in Access Point mode.

## Hardware configuration

In the project I used the Raspberry Pi Pico W with RP2040 microcontroller and Infineon CYW43439 Wifi and Bluetooth chip, see the device datasheet as reference.

The temperature sensor is connected to the I2C channel 0 using:
- SDA on GPIO 8
- SCL on GPIO 9

The EEPROM flash memory is connected to the I2C channel 1 using:
- SDA on GPIO 14
- SCL on GPIO 15

The UART0 is used to transmitt the measured values; the pin used are:
- UART TX on GPIO 0
- UART RX on GPIO 1

The GPIO22 is used to setup the wifi mode according to these values:
- if GPIO 22 is high the device works in Station Mode (it needs to have a valid SSID and password to connect to the wifi network).
- if GPIO 22 is low the device works in Access Point Mode.

In Access Point Mode, the default network SSID is "PICOW-WIFI" and the password is "PicoWifiPass".
The default IP address of the device is 192.168.8.1 and it acts as DHCP server for up to 4 clients.

The RGB LED is connected to the following GPIOs:
- Led GREEN on GPIO 3
- Led RED on GPIO 4
- Led BLU on GPIO 5

Depending on the LED used, it is necessary to add resistors between the LED and the Pico PIN to limit the current.
I used the AZDelivery KY-009 RGB LED.

Obviously all the connections and PINs used can be modified by acting on the defines present in the code.


## Web interface

The simple web server implemented in the device serves this pages:

| Page              | Available in AP mode | Avilable in STA mode |
|-------------------|----------------------|----------------------|
| /info             |           YES        |        YES           |
| /settings         |           YES        |        NO            |
| /advparams        |           YES        |        NO            |

There are other pages to set the thresholds but they are not implemented yet.

The response to /info request is the following:
![info](/resources/info.jpg "info web page response")


The response to /settings page depends on the wifi mode; in AP mode the response is:
![settings](/resources/settings_ap.jpg "settings web page response in AP mode")


In Station mode (STA), the response is:
![settings](/resources/settings_sta.jpg "settings web page response in STA mode")


The advanced configuration page /advparams is not ready yet, but the response, now, is like this:
![advparams](/resources/advparams_ap.jpg "settings web page response")


NOTE: at the moment the form use the GET method.

Simple web pages use a style sheet that can be easily modified to change the look of the pages.

## API interface

The device also implements an API endpoints.
The implementation is in progess.
The table below show the API planned and the staus of implementation.


| API request           |   Method  | Implemented |
|-----------------------|-----------|-------------|
| /api/v1/info          |    GET    |   YES       |
| /api/v1/settings      |    GET    |   YES       |
| /api/v1/setparams     |    N/A    |   NOT YET   |
| /api/v1/setadvparams  |    N/A    |   NOT YET   |
| /api/v1/sethightemp   |    N/A    |   NOT YET   |
| /api/v1/setlowtemp    |    N/A    |   NOT YET   |
| /api/v1/sethighhum    |    N/A    |   NOT YET   |
| /api/v1/setlowhum     |    N/A    |   NOT YET   |


Here an example of the response to API request /api/v1/info
![api info](/resources/info_api.jpg "API info response")

Here an example of the response to API request /api/v1/settings
![api settings](/resources/settings_api.jpg "API settings response")


## Serial interface

To view the serial output of the device it's enough to connect the UART's PIN to an UART-to-USB converter (TX of the device on RX of the converter): the ouput will be available on a terminal console on the PC.

Be careful to use a 3.3V converter!

I use AZ-Delivery FT232RL with putty on Window 11 PC and it works.

Here the output of the UART interface (in this case in CSV format *Temperature;Humidity*)
![wlt in action](/resources/serial_output.jpg "the CSV format out log")



