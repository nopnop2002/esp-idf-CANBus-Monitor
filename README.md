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

# Software requirement    
esp-idf v4.2-dev-2243 or later.   
Use twai(Two-Wire Automotive Interface) driver instead of can driver.   

# Hardware requirements

1. Windows PC   
Windows applications can be downloaded here.
   - [V7.10](https://github.com/SeeedDocument/USB-CAN_Analyzer/tree/master/res/USB-CAN%20software%20and%20drive(v7.10)/Program)   
   - [V7.20](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/V7.20)   
   - [V8.00](https://github.com/SeeedDocument/USB-CAN-Analyzer/tree/master/res/Program)   

2. USB-TTL Converter   
ESP32 development board has USB.   
This USB connects to Linux and is used for writing the firmware and displaying the LOG.   
Need converter to connect with Windows PC.   

|Converter||ESP32|
|:-:|:-:|:-:|
|RXD|--|GPIO4|
|TXD|--|GPIO5|
|GND|--|GND|


3. SN65HVD23x CAN-BUS Transceiver   

|SN65HVD23x||ESP32||
|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO21||
|GND|--|GND||
|Vcc|--|3.3V||
|R(CRX)|--|GPIO22||
|Vref|--|N/C||
|CANL|--||To CAN Bus|
|CANH|--||To CAN Bus|
|RS|--|GND(*1)||

(*1) N/C for SN65HVD232

4. Termination resistance   
I used 150 ohms.   

# Test Circuit   
```
   +-----------+ +-----------+ +-----------+ 
   | Atmega328 | | Atmega328 | |   ESP32   | 
   |           | |           | |           | 
   | Transmit  | | Receive   | | 21    22  | 
   +-----------+ +-----------+ +-----------+ 
     |       |    |        |     |       |   
   +-----------+ +-----------+   |       |   
   |           | |           |   |       |   
   |  MCP2515  | |  MCP2515  |   |       |   
   |           | |           |   |       |   
   +-----------+ +-----------+   |       |   
     |      |      |      |      |       |   
   +-----------+ +-----------+ +-----------+ 
   |           | |           | | D       R | 
   |  MCP2551  | |  MCP2551  | |   VP230   | 
   | H      L  | | H      L  | | H       L | 
   +-----------+ +-----------+ +-----------+ 
     |       |     |       |     |       |   
     +--^^^--+     |       |     +--^^^--+
     |   R1  |     |       |     |   R2  |   
 |---+-------|-----+-------|-----+-------|---| BackBorn H
             |             |             |
             |             |             |
             |             |             |
 |-----------+-------------+-------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

# Install   
```
git clone https://github.com/nopnop2002/esp-idf-CANBus-Monitor
cd esp-idf-CANBus-Monitor
make menuconfig
make flash
```

# Configure   

![config-1](https://user-images.githubusercontent.com/6020549/87838761-096cc780-c8d3-11ea-9fdb-d82ef5b7ce1f.jpg)

![config-2](https://user-images.githubusercontent.com/6020549/87838768-0c67b800-c8d3-11ea-83f4-3635688b44bb.jpg)

# How to use   
- Write firmware to ESP32.   
- Connect ESP32 and Windows PC using USB-TTL Converter.   
- Start a Windows application.   

# User manual   
See [here](https://github.com/nopnop2002/esp-idf-CANBus-Monitor/tree/master/UserManual).   

# CAN receive message transfer   
CAN receive messages are broadcast using UDP.   
You can use recv.py.   
![USBCAN-501](https://user-images.githubusercontent.com/6020549/87840019-78005400-c8d8-11ea-9d68-e71a846fbc0b.jpg)