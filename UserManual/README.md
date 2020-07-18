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
![USBCAN-121](https://user-images.githubusercontent.com/6020549/86522579-2e0c7c80-be9b-11ea-93c4-789fe90c7ba0.jpg)   

![USBCAN-123](https://user-images.githubusercontent.com/6020549/86522578-2cdb4f80-be9b-11ea-8382-a61c3e8d6bbf.jpg)   
The left mouse button double click it.   

## Configure Receive ID
![USBCAN-201](https://user-images.githubusercontent.com/6020549/86522621-a2472000-be9b-11ea-9fcb-0f9617ee1cca.jpg)   

![USBCAN-203](https://user-images.githubusercontent.com/6020549/86522613-9e1b0280-be9b-11ea-8b66-08c2d0242829.jpg)   

![USBCAN-205](https://user-images.githubusercontent.com/6020549/86522615-9f4c2f80-be9b-11ea-9883-dad451b7c679.jpg)   

![USBCAN-207](https://user-images.githubusercontent.com/6020549/86522616-9fe4c600-be9b-11ea-9f8c-7962921899fb.jpg)   

![USBCAN-209](https://user-images.githubusercontent.com/6020549/86522618-a07d5c80-be9b-11ea-8cf0-9fb0095a5e1f.jpg)   
Display only valid IDs.   

![USBCAN-211](https://user-images.githubusercontent.com/6020549/86522619-a115f300-be9b-11ea-917d-a748f48b8154.jpg)   

![USBCAN-213](https://user-images.githubusercontent.com/6020549/86522620-a1ae8980-be9b-11ea-8afd-55145993f4da.jpg)   

## Disable Configure
![USBCAN-301](https://user-images.githubusercontent.com/6020549/86522644-1da8d180-be9c-11ea-89df-43e2cc41e040.jpg)   

![USBCAN-303](https://user-images.githubusercontent.com/6020549/86522643-1c77a480-be9c-11ea-8987-f43d12fef22b.jpg)   


## Send data frame
![USBCAN-401](https://user-images.githubusercontent.com/6020549/86522649-21d4ef00-be9c-11ea-9834-763b0f663dc2.jpg)   

![USBCAN-403](https://user-images.githubusercontent.com/6020549/86522645-20a3c200-be9c-11ea-9f61-e777b218aaaa.jpg)   

![USBCAN-405](https://user-images.githubusercontent.com/6020549/86522647-213c5880-be9c-11ea-89ec-f5fcc99b5619.jpg)   

# Send remote frame
![USBCAN-501](https://user-images.githubusercontent.com/6020549/86522677-9b6cdd00-be9c-11ea-8d17-0fa9701c473e.jpg)   

![USBCAN-503](https://user-images.githubusercontent.com/6020549/86522678-9c057380-be9c-11ea-9cf1-2cc7bec23c3f.jpg)   

![USBCAN-505](https://user-images.githubusercontent.com/6020549/86522676-9a3bb000-be9c-11ea-9ed2-c00653c94498.jpg)   


## Send Cycle
![USBCAN-601](https://user-images.githubusercontent.com/6020549/86522691-d8d16a80-be9c-11ea-99dc-d2cc417d83d1.jpg)   

![USBCAN-603](https://user-images.githubusercontent.com/6020549/86522693-d96a0100-be9c-11ea-8534-33a425800970.jpg)   

![USBCAN-605](https://user-images.githubusercontent.com/6020549/86522694-da029780-be9c-11ea-89e2-528a2779fd58.jpg)   

![USBCAN-607](https://user-images.githubusercontent.com/6020549/86581499-1fb58200-bfbb-11ea-8ce7-3d79676d2130.jpg)   

![USBCAN-609](https://user-images.githubusercontent.com/6020549/86522687-d707a700-be9c-11ea-9f57-4deeb4618c65.jpg)   

![USBCAN-611](https://user-images.githubusercontent.com/6020549/86522689-d7a03d80-be9c-11ea-9f32-17db57f3fa57.jpg)   

![USBCAN-613](https://user-images.githubusercontent.com/6020549/86522690-d838d400-be9c-11ea-8121-4b99af4d5f8b.jpg)   


## Monitor Error Counter
![USBCAN-651](https://user-images.githubusercontent.com/6020549/86579304-170f7c80-bfb8-11ea-92ee-15af6f244695.jpg)   


## Grid control
![USBCAN-701](https://user-images.githubusercontent.com/6020549/86522723-49788700-be9d-11ea-99f6-3f0bcb08dd98.jpg)   

![USBCAN-703](https://user-images.githubusercontent.com/6020549/86522724-4aa9b400-be9d-11ea-9a31-1f2a9a0dfbc7.jpg)   


