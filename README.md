# WiFi Light Thermo (wlt)  

> NOTE: This project is based on the Raspberry PICO example [tcp_server](https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/tcp_server/).  

WiFi Light Thermo, implements a WiFi-enabled thermostat that measures temperature and humidity.  
The measured values can be accessed via a web server interface — either through a simple formatted web page, an API endpoint, or a serial log.  
The device allows configuration of several parameters, including:  
- The device name  
- Temperature format: °C or °F  
- Output format on serial port: TXT or CSV
- Theme of the home page (dark or light)  
- Sensor polling interval
- Temperature or humidity associated with the output  
- Temperature and humidity thresholds to activate or deactivate associated outputs, see [`Remarks`](#remarks).  

You can also set the SSID and password to connect the device to a desired WiFi network.  
All configuration settings are stored in EEPROM memory connected via the I2C bus: I use a memory add-on available in my project [addon_eeprom_i2c](https://github.com/montif1975/addon_eeprom_i2c).  
The device's WiFi mode — Access Point (AP) or Station (STA) — is selected based on the state of a predefined GPIO pin (default: GPIO 22).  

To know the working status of the device, I added a RGB led that shows these colors according to the status:  

| LED COLOR | Type  | Status of device |
|---------- |-------|------------------|
| Red       | Solid | BOOT IN PROGRESS |
| Red       | Blink | GENERIC ERROR    |
| Green     | Blink | RUN IN STA MODE  |
| Blue      | Blink | RUN IN AP MODE   |
| Yellow    | Solid | WIFI FAIL        |  

Sensor readings are always available, but configuration via web server is only possible when the device is operating in Access Point mode.  
Using API, is possible to change configuration also in Station Mode.  

## Hardware configuration  

In the project I used the Raspberry Pi Pico W with RP2040 microcontroller and Infineon CYW43439 Wifi and Bluetooth chip, see the device datasheet as reference.  
The GPIO used in the code are indicated in the following table.  

|       FUNCTION        | SIGNAL    | GPIO  |
|-----------------------|-----------|-------|
| Temperature sensor    | SDA       |   8   |
|                       | SCL       |   9   |
| EEPROM flash memory   | SDA       |   14  |
|                       | SCL       |   15  |
| UART0 (log)           | TX        |   0   |
|                       | RX        |   1   |
| Wifi mode selector    | IN        |   22  |
| RGB Led green         | OUT       |   3   |
| RGB Led red           | OUT       |   4   |
| RGB Led blue          | OUT       |   5   |
| Output 1              | OUT       |   6   |
| Output 2              | OUT       |   7   |

The GPIO22 is used to setup the wifi mode according to these values:  
- if GPIO 22 is high the device works in Station Mode (it needs to have a valid SSID and password to connect to the wifi network).  
- if GPIO 22 is low the device works in Access Point Mode.  

In Access Point Mode, **the default network SSID is "PICOW-WIFI" and the password is "PicoWifiPass"**.  
The default IP address of the device is **192.168.8.1** and it acts as DHCP server for up to 4 clients.  

Depending on the LED used, it is necessary to add resistors between the LED and the Pico PIN to limit the current.  
I used the AZDelivery KY-009 RGB LED.  

> Obviously all the connections and PINs used can be modified by acting on the defines present in the code.  

## Web interface  

The simple web server implemented in the device serves this pages:  

| Page              | Available in AP mode | Available in STA mode |
|-------------------|----------------------|-----------------------|
| /home             |           YES        |        YES            |
| /settings         |           YES        |        NO             |
| /advparams        |           YES        |        NO             |  

`/home` is available in dark or light theme.  

The dark theme is this:  
![home](/resources/home_dark.jpg "home web page in dark mode")  

The light theme is this:  
![home](/resources/home_light.jpg "home web page in light mode")  

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
| /api/v1/setoutparams     |    POST   |   YES       |  

If the API requested is not implemented, the device replies with a `501 - Not Implemented` http code.  

### /api/v1/info  
The `/api/v1/info` is used to get the last read from the sensor: in the response's body there is a JSON with the temperature, the format of the temperature and the umididity value.  
The body of the replay is:  
```json  
{
    "T": 28.75,
    "TF": "C",
    "H": 49.88
}
```  
where:  
- "T" = Temperature value in TF format
- "TF" = Temperature format can be one of the following values:  
    - "C" = Celsius degree  
    - "F" = Fahrenheit degree  
- "H" = Humidity value in %RH  

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
    "OUTS":[
        {
            "GPIO":6,
            "DT":"T",
            "TH":26.00,
            "TR":"H"
        },
        {
            "GPIO":7,
            "DT":"H",
            "TH":61.50,
            "TR":"H"
        }
    ]
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
        "MODE": "STA",
        "SSID": "the SSID of the wifi network",
        "PASS": "the password of the wifi network"
    }
}
```  
where:  
- "DEVNAME" = String to identify the device (the name of the device)  
- "MODE" = WiFi mode, can be one of the following values:  
    - "STA" = Station mode  
    - "AP" = Access point mode  
- "SSID" = SSID of the WiFi network  
- "PASS" = Password of the WiFi network  
If at least one of the value for the expected keys has a wrong value, the device replies with a `400 - Bad Request`.  

### /api/v1/setsettingparams  
The `/api/v1/setsettingparams` body request is:
```json
{
    "SETTINGS": {
        "TF": "F",
        "OF": "TXT",
        "PT": 30,
        "TH": 3,
        "WT": "DARK"
    }
}
```  
where:  
- "TF" = Temperature scale, can be one of the following values:  
    - "C" = Celsius degree  
    - "F" = Fahrenheit degree  
- "OF" = Output format on serial port:  
    - "TXT" = text format: XX.YY °C/°F - XX,YY %RH  (es. 35.25 °C - 45.50 %RH)  
    - "CSV" = Comma Separated Values: temp;hum (es. 23.45;57.45)  
- "PT" = Polling time in 1..63 seconds range  
- "TH" = Thresholds Hysteresis in 1..7 polling time range (it's the number of polling time wait before update the output state in order to avoid continuous and fast switching)  
- "WT" = Web page theme, can be one the following values:  
    - "DARK"  
    - "LIGHT"  

### /api/v1/setoutparams  
The `/api/v1/setoutparams` body request is:  
```json
{
    "OUTS":[
        {
            "GPIO":6,
            "DT":"T",
            "TH":26.00,
            "TR":"H"
        },
        {
            "GPIO":7,
            "DT":"H",
            "TH":61.50,
            "TR":"H"
        }
    ]
}
```  
where:  
- "GPIO" = number of the GPIO used (depends on PIN connected)  
- "DT" = Data type, can be one of the following values:  
    - T = Temperature  
    - H = Humidity  
    - P = Pressure (only if sensor support it)  
- "TH" = Threshold value  
- "TR" = type of the trigger, can be one of the following value:  
    - "NONE" = no-threshold, disabled  
    - "H" = high, when the value is higher than VAL  
    - "L" = low, when the value is lower than VAL  

> NOTE: OUTS must be a Json Array even if it contains the settings of only one output.  

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


