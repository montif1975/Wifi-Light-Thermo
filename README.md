# WiFi Light Thermo (wlt)  

> NOTE: This project is based on the Raspberry PICO example [tcp_server](https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/tcp_server/).  

WiFi Light Thermo, implements a WiFi-enabled thermostat that measures temperature and humidity.  
The measured values can be accessed via a web server interface — either through a simple formatted web page, an API endpoint, or a serial log.  
The device allows configuration of several parameters, including:  
- The device name  
- Temperature format: °C or °F  
- Output format: TXT or CSV
- Theme of the home page (dark or light)  
- Sensor polling interval  
- Temperature and humidity thresholds to activate or deactivate predefined outputs, see [`Remarks`](#remarks).  

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

In Access Point Mode, **the default network SSID is "PICOW-WIFI" and the password is "PicoWifiPass"**.  
The default IP address of the device is **192.168.8.1** and it acts as DHCP server for up to 4 clients.  

The RGB LED is connected to the following GPIOs:  
- Led GREEN on GPIO 3  
- Led RED on GPIO 4  
- Led BLU on GPIO 5  

Depending on the LED used, it is necessary to add resistors between the LED and the Pico PIN to limit the current.  
I used the AZDelivery KY-009 RGB LED.  

> Obviously all the connections and PINs used can be modified by acting on the defines present in the code.  

## Web interface  

The simple web server implemented in the device serves this pages:  

| Page              | Available in AP mode | Available in STA mode |
|-------------------|----------------------|-----------------------|
| /home  (*)        |           YES        |        YES            |
| /info             |           YES        |        YES            |
| /settings         |           YES        |        NO             |
| /advparams        |           YES        |        NO             |  

`/home` is available in dark or light theme.  
The dark theme is this:  
![home](/resources/home_dark.jpg "home web page in dark mode")  
The light theme is this:  
![home](/resources/home_light.jpg "home web page in light mode")  

The response to `/info` request is the following:  
![info](/resources/info.jpg "info web page response")  

The response to `/settings` page depends on the wifi mode; in AP mode the response is:  
![settings](/resources/settings_ap.jpg "settings web page response in AP mode")  

In Station mode (STA), the response is:  
![settings](/resources/settings_sta.jpg "settings web page response in STA mode")  

The advanced configuration page `/advparams` is not ready yet, but the response, now, is like this:  
![advparams](/resources/advparams_ap.jpg "settings web page response")  
NOTE: all the settings can be changed use API request described in the next section.  

> Simple web pages use a style sheet that can be easily modified to change the look of the pages.  

## API interface  

The device also implements an API endpoints.  
The table below lists the API implementated.  

| API request              |   Method  | Implemented |
|--------------------------|-----------|-------------|
| /api/v1/info             |    GET    |   YES       |
| /api/v1/settings         |    GET    |   YES       |
| /api/v1/setallparams     |    POST   |   YES       |
| /api/v1/setwifiparams    |    POST   |   YES       |
| /api/v1/setsettingparams |    POST   |   YES       |
| /api/v1/setthreshparams  |    POST   |   YES       |  

If the API requested is not implemented, the device replies with a `501 - Not Implemented` http code.  

### /api/v1/info  
The `/api/v1/info` is used to get the last read from the sensor: in the response's body there is a JSON with the temperature, the format of the temperature and the umididity value.  
The body of the replay is:  
```json  
{
    "T": 28.75,
    "TF": "C" or "F",
    "H": 49.88
}
```  
where:  
- "C" = Celsius degree  
- "F" = Fahrenheit degree  

### /api/v1/settings  
The `/api/v1/settings` is used to get all the configuration of the device.  
The response's body is:  
```json  
{
    "WIFI":{
        "DEVNAME":"my office",
        "SSID":"my wifi network",
        "MODE":"STA",
        "IPADDR":"192.168.1.2",
        "NET":"255.255.255.0",
        "GW":"192.168.1.1"
    },
    "SETTINGS":{
        "TF":"C",
        "OF":"CSV",
        "PT":30,
        "TH":3,
        "WT":"DARK"
    },
    "THRESH":{
        "HTT":{
            "VAL":30,
            "TR":"H"
        },
        "HTH":{
            "VAL":65,
            "TR":"H"
        },
        "LTT":{
            "VAL":18,
            "TR":"L"
        },
        "LTH":{
            "VAL":45,
            "TR":"L"
        }
    }
}
```  
> In the WIFI object the IP address, the netmask and the gateway value are assigned by the network when the device is in STA (station) mode.  

### /api/v1/setallparams  
The `/api/v1/setallparams` parse all the settings that finds in the body of the request.  
The body must be a JSON with one, two or all three keys expected by the `/api/v1/setXXXparams` described int the next sections.  
If at least one of the value for the expected keys has a wrong value, the device replies with a `400 - Bad Request`.  

### /api/v1/setwifiparams  
The `/api/v1/setwifiparams` body request is:  
```json
{
    "WIFI": {
        "DEVNAME": "the name of the device",
        "MODE": "STA" or "AP",
        "SSID": "the SSID of the wifi network",
        "PASS": "the password of the wifi network"
    }
}
```  
where:  
- "STA" = Station mode  
- "AP" = Access point mode  
If at least one of the value for the expected keys has a wrong value, the device replies with a `400 - Bad Request`.  

### /api/v1/setsettingparams  
The `/api/v1/setsettingparams` body request is:
```json
{
    "SETTINGS": {
        "TF": "F" or "C", # temperature format
        "OF": "TXT" or "CSV", # output format
        "PT": 1..63, # polling time in s
        "TH": 1..7, # threshold Hysteresis
        "WT": "DARK" or "LIGHT", # web page theme
    }
}
```  
where:  
- "C" = Celsius degree  
- "F" = Fahrenheit degree  
- "TXT" = text format: XX.YY °C/°F - XX,YY %RH  (es. 35.25 °C - 45.50 %RH)  
- "CSV" = Comma Separated Values: temp;hum (es. 23.45;57.45)  

### /api/v1/setthreshparams  
The `/api/v1/setthreshparams` body request is:  
```json
{
    "THRESH": {
        "HTT": {
            "VAL": -40 .. +125,
            "TR": "NONE" | "H" | "L" | "B"
        },
        "HTH": {
            "VAL": 0 .. 100,
            "TR": "NONE" | "H" | "L" | "B"
        },
        "LTT": {
            "VAL": -40 .. +125,
            "TR": "NONE" | "H" | "L" | "B"
        },
        "LTH": {
            "VAL": 0 .. 100,
            "TR": "NONE" | "H" | "L" | "B"
        }
    }
}
```  
where:  
- "HTT" = High threshold temperature  
- "HTH" = High threshold humidity  
- "LTT" = Low threshold temperature  
- "LTH" = Low threshold humidity  
- "VAL" = value of the threshold. **The temperature thresholds are be interpreted as °C**  
- "TR" = type of the trigger, can be one of the following value:  
    - "NONE" = no-threshold, disabled  
    - "H" = high, when the value is higher than VAL  
    - "L" = low, when the value is lower than VAL  
    - "B" = both, the action associated with the threshold is executed both when the physical quantity becomes greater than VAL and when it becomes less.  

## Serial interface  

To view the serial output of the device it's enough to connect the UART's PIN to an UART-to-USB converter (TX of the device on RX of the converter): the ouput will be available on a terminal console on the PC.  

> Be careful to use a 3.3V converter!  

I use AZ-Delivery FT232RL with putty on Window 11 PC and it works.  
Here the output of the UART interface (in this case in CSV format *Temperature;Humidity*)  
![wlt in action](/resources/serial_output.jpg "the CSV format out log")  

## Remarks  

Web Server:  
- The advanced configuration page /advparams is not ready yet  
- there are other pages to set the thresholds but they are not implemented yet.  


