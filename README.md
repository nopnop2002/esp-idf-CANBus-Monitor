# esp-idf-CANBus-Monitor
Monitor Canbus traffic.

![USBCAN-V800](https://user-images.githubusercontent.com/6020549/86580074-2cd17180-bfb9-11ea-99b5-16e32ab5cc47.jpg)

# Background

Windows application for monitoring CANBus is published [here](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/Program).   
This tool is designed to be used with this hardware.   
![bazaar487719_1](https://user-images.githubusercontent.com/6020549/86521939-b9811000-be91-11ea-8dfc-2bc8e24d7f0c.jpg)

So, I made the hardware side with ESP32.   


# Application structure
```
+-------------+           +-------------+           +-------------+            +-------------+
| Windows     |           |   USB-TTL   |           |             |            | SN65HVD23x  |
| Application |<--(USB)-->|  Converter  |<--(TTL)-->|    ESP32    |<--(TWAI)-->|   CAN-BUS   |<--(CAN BUS)-->CAN Network
|             |           |             |           |             |            | Transceiver |
+-------------+           +-------------+           +-------------+            +-------------|
```
On the ESP32S2/S3, you can use USB instead of UART.   
```
+-------------+                                     +-------------+            +-------------+
| Windows     |                                     |             |            | SN65HVD23x  |
| Application |<--------------(USB)---------------->| ESP32S2/S3  |<--(TWAI)-->|   CAN-BUS   |<--(CAN BUS)-->CAN Network
|             |                                     |             |            | Transceiver |
+-------------+                                     +-------------+            +-------------|
```

# Software requirement    
ESP-IDF V5.0 or later.   
ESP-IDF V4.4 release branch reached EOL in July 2024.   

# Hardware requirements

1. Windows PC   
Windows applications can be downloaded from here.
   - [V7.10](https://github.com/SeeedDocument/USB-CAN_Analyzer/tree/master/res/USB-CAN%20software%20and%20drive(v7.10)/Program)   
   - [V7.20](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/V7.20)   
   - [V8.00](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/Program)   

2. USB-TTL Converter    
ESP32 development board has USB.   
This USB connects to Linux and is used for writing the firmware and displaying the LOG.   
Need converter to connect with Windows PC.   

|Converter||ESP32|ESP32-S2/S3|ESP32-C3/C6||
|:-:|:-:|:-:|:-:|:-:|:-:|
|RXD|--|GPIO4|GPIO4|GPIO4|(*1)|
|TXD|--|GPIO5|GPIO5|GPIO5|(*1)|
|GND|--|GND|GND|GND||

(*1) You can change using menuconfig.   

On the ESP32S2/S3, you can use USB instead of UART.   
I used this USB Connector.   
![usb-connector](https://user-images.githubusercontent.com/6020549/124848149-3714ba00-dfd7-11eb-8344-8b120790c5c5.JPG)

```
ESP32-S2/S3 BOARD          USB CONNECTOR
                           +--+
                           | || VCC
    [GPIO 19]    --------> | || D-
    [GPIO 20]    --------> | || D+
    [  GND  ]    --------> | || GND
                           +--+
```

3. SN65HVD23x CAN-BUS Transceiver   
SN65HVD23x series has 230/231/232.   
They differ in standby/sleep mode functionality.   
Other features are the same.   

4. Termination resistance   
I used 150 ohms.   

# Wireing   
|SN65HVD23x||ESP32|ESP32-S2/S3|ESP32-C3/C6||
|:-:|:-:|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO21|GPIO0|GPIO0|(*1)|
|GND|--|GND|GND|GND||
|Vcc|--|3.3V|3.3V|3.3V||
|R(CRX)|--|GPIO22|GPIO1|GPIO1|(*1)|
|Vref|--|N/C|N/C|N/C||
|CANL|--||||To CAN Bus|
|CANH|--||||To CAN Bus|
|RS|--|GND|GND|GND|(*2)|

(*1) You can change using menuconfig.   

(*2) N/C for SN65HVD232

# Test Circuit   
```
   +-----------+   +-----------+   +-----------+ 
   | Atmega328 |   | Atmega328 |   |   ESP32   | 
   |           |   |           |   |           | 
   | Transmit  |   | Receive   |   | 21    22  | 
   +-----------+   +-----------+   +-----------+ 
     |       |      |        |       |       |   
   +-----------+   +-----------+     |       |   
   |           |   |           |     |       |   
   |  MCP2515  |   |  MCP2515  |     |       |   
   |           |   |           |     |       |   
   +-----------+   +-----------+     |       |   
     |      |        |      |        |       |   
   +-----------+   +-----------+   +-----------+ 
   |           |   |           |   | D       R | 
   |  MCP2551  |   |  MCP2551  |   |   VP230   | 
   | H      L  |   | H      L  |   | H       L | 
   +-----------+   +-----------+   +-----------+ 
     |       |       |       |       |       |   
     +--^^^--+       |       |       +--^^^--+
     |   R1  |       |       |       |   R2  |   
 |---+-------|-------+-------|-------+-------|---| BackBorn H
             |               |               |
             |               |               |
             |               |               |
 |-----------+---------------+---------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

# Installation
```
git clone https://github.com/nopnop2002/esp-idf-CANBus-Monitor
cd esp-idf-CANBus-Monitor
idf.py set-target {esp32/esp32s2/esp32s3/esp32c3/esp32c6}
idf.py menuconfig
idf.py flash
```

# Configuration
![config-main](https://user-images.githubusercontent.com/6020549/126859035-9c83d0b7-14ba-4bc0-8246-0a394887cffa.jpg)
![config-app](https://github.com/nopnop2002/esp-idf-CANBus-Monitor/assets/6020549/3dd5b3d2-ce48-4881-ac67-36eb4e45fefc)

## CAN Setting
![config-can](https://user-images.githubusercontent.com/6020549/126859050-bf104e35-c149-4059-9182-f4f9d2e2f853.jpg)

## UART Setting
- Using UART   
	![Image](https://github.com/user-attachments/assets/9d51cec4-e042-44f2-bb54-6563cf0e8245)
- Using USB   
	![Image](https://github.com/user-attachments/assets/fbf99b97-608d-4f49-b1c7-777362310bc6)

# How to use   
- Add ESP32 to CanBus.   
- Write firmware to ESP32.   
- Connect ESP32 and Windows PC using USB-TTL Converter or USB.   
- Start a Windows application.   

# Windows application User manual   
See [here](https://github.com/nopnop2002/esp-idf-CANBus-Monitor/tree/master/UserManual).   

Official document is [here](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/Document).


# Reference   

https://github.com/nopnop2002/esp-idf-candump

https://github.com/nopnop2002/esp-idf-can2http

https://github.com/nopnop2002/esp-idf-can2mqtt

https://github.com/nopnop2002/esp-idf-can2usb

https://github.com/nopnop2002/esp-idf-can2websocket

https://github.com/nopnop2002/esp-idf-can2socket
