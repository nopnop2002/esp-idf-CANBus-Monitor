# esp-idf-CANBus-Monitor Users Manual

## Connect to ESP32

![USBCAN-101](https://user-images.githubusercontent.com/6020549/87839201-c875b280-c8d4-11ea-9a82-096466d899b5.jpg)

![USBCAN-103](https://user-images.githubusercontent.com/6020549/87839207-cc093980-c8d4-11ea-8bc6-2bc469acfebc.jpg)
**COM bps only supports 115200.**   

![USBCAN-105](https://user-images.githubusercontent.com/6020549/87839209-cf042a00-c8d4-11ea-9625-58c34d100635.jpg)

## Connect to CAN-Network

![USBCAN-107](https://user-images.githubusercontent.com/6020549/87839213-d1668400-c8d4-11ea-9ffd-4a66da519a66.jpg)
Mode: This is not supported.   
CAN bps: Some do not support it.   
- [x] 1M   
- [ ] 800K   
- [x] 500K   
- [ ] 400K   
- [x] 250K   
- [x] 200K   
- [x] 125K   
- [x] 100K   
- [x] 50K   
- [x] 20K   
- [x] 10K   
- [x] 5K   

Manual set bps: This is not supported.   
Only send once: This is not supported.   
Fixed 20 bytes to send and receive:This is not supported.   

![USBCAN-109](https://user-images.githubusercontent.com/6020549/87839214-d3c8de00-c8d4-11ea-87ae-5ca418f31f34.jpg)
Filter ID and Mask ID: Hexadecimal data filtering the IDs and Mask ID.   
For Standard frame, the lower 11 bits are valid (range: 0x00000000 to 0x000007ff).   
For Extended frames, 29 bits are valid (range 0x00000000 to 0x1fffffff).   

![USBCAN-111](https://user-images.githubusercontent.com/6020549/87839217-d62b3800-c8d4-11ea-85f9-ac75e878d9a6.jpg)

![USBCAN-113](https://user-images.githubusercontent.com/6020549/87839224-db888280-c8d4-11ea-9821-aecfea110c0b.jpg)


## View Detail
![USBCAN-121](https://user-images.githubusercontent.com/6020549/87839454-d37d1280-c8d5-11ea-94f6-e156a86641c4.jpg)


## Configure The Receive ID
![USBCAN-131](https://user-images.githubusercontent.com/6020549/87839545-340c4f80-c8d6-11ea-989a-de92d7ac2fd9.jpg)

![USBCAN-133](https://user-images.githubusercontent.com/6020549/87839546-34a4e600-c8d6-11ea-988b-2920d089e94c.jpg)

![USBCAN-135](https://user-images.githubusercontent.com/6020549/87839536-2eaf0500-c8d6-11ea-8493-db13523cfae6.jpg)

![USBCAN-137](https://user-images.githubusercontent.com/6020549/87839538-2fe03200-c8d6-11ea-9080-f785b6182cf7.jpg)

![USBCAN-139](https://user-images.githubusercontent.com/6020549/87839539-3078c880-c8d6-11ea-9138-cfcad11eb691.jpg)
Only upload configured ID.   

![USBCAN-141](https://user-images.githubusercontent.com/6020549/87839540-31115f00-c8d6-11ea-9e58-4184a1a656b1.jpg)

![USBCAN-143](https://user-images.githubusercontent.com/6020549/87839541-31a9f580-c8d6-11ea-88eb-c1d33463461e.jpg)

![USBCAN-145](https://user-images.githubusercontent.com/6020549/87839542-32db2280-c8d6-11ea-9a92-e376ba0f9d94.jpg)
Receive ID not uploaded.

![USBCAN-147](https://user-images.githubusercontent.com/6020549/87839543-3373b900-c8d6-11ea-8c54-d535c0b28f29.jpg)

![USBCAN-149](https://user-images.githubusercontent.com/6020549/87839636-abda7a00-c8d6-11ea-9145-e4b9db2fd9f2.jpg)
Disable configure.   


## Send data frame
![USBCAN-151](https://user-images.githubusercontent.com/6020549/87839717-0c69b700-c8d7-11ea-8df7-384beb079857.jpg)

![USBCAN-153](https://user-images.githubusercontent.com/6020549/87839713-0a9ff380-c8d7-11ea-8c45-e06230616046.jpg)

![USBCAN-155](https://user-images.githubusercontent.com/6020549/87839716-0bd12080-c8d7-11ea-93ac-bfa86f1e735c.jpg)


# Send remote frame
![USBCAN-161](https://user-images.githubusercontent.com/6020549/87839737-1f7c8700-c8d7-11ea-9bd3-7bb5bf1d67d7.jpg)

![USBCAN-163](https://user-images.githubusercontent.com/6020549/87839739-23100e00-c8d7-11ea-93e7-0228d342ac64.jpg)

![USBCAN-165](https://user-images.githubusercontent.com/6020549/87839743-26a39500-c8d7-11ea-9bf6-55b11a65af01.jpg)


## Send Cycle & Sequence


## Monitor Error Counter


## Grid control


