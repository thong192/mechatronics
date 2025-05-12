1. Trần Nguyên Khôi - 22146158
2. Lê Thanh Thông - 22146235
3. Đặng Phước Thịnh - 22146233


Overview:
========
This document explains how to install, configure, and use 
the BMP180 device driver for the BMP180 sensor. This driver
provides access to the sensor's functionality via a character 
device and supports reading temperature and pressure data through 
ioctl calls.

    #include <stdio.h>
    #include <stdlib.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <errno.h>
    #include <string.h>

Key Features:
============
    * Device: BMP180, a digital barometer from Bosch, measures 
    atmospheric pressure and temperature.
    * Functionality: The driver supports reading temperature (°C) 
    and pressure (Pa) using the ioctl interface.
    * Kernel Module: This driver is implemented as a kernel module 
    and exposes a character device interface for communication.

Device Interface:
================
    Device File:
    ===========
        The driver creates a character device file /dev/bmp180 which 
        allows user-space applications to interact with the BMP180 sensor.

    Supported IOCTL Operations:
    ==========================
        The BMP180 driver provides two main ioctl commands to access sensor data:

        #define BMP180_IOCTL_MAGIC        'b'
        #define BMP180_IOCTL_READ_TEMP    _IOR(BMP180_IOCTL_MAGIC, 1, int)
        #define BMP180_IOCTL_READ_PRESSURE _IOR(BMP180_IOCTL_MAGIC, 2, int)

    1. Read Temperature (BMP180_IOCTL_READ_TEMP):

        Command: BMP180_IOCTL_READ_TEMP
        Operation: Reads the current temperature from the BMP180 sensor.
        Return Value: The temperature in 0.1°C.

    2. Read Pressure (BMP180_IOCTL_READ_PRESSURE):

        Command: BMP180_IOCTL_READ_PRESSURE
        Operation: Reads the current pressure from the BMP180 sensor.
        Return Value: The pressure in Pascals (Pa).

NOTES:
=====
    Make sure the BMP180 sensor is connected to I2C1
    The default I2C address of the BMP180 is 0x77.
    Return value is in Pa (Pascal). To convert to hPa (hectopascal), divide by 100.
Steps to Install:
================
    Step 1: Enable I2C on Raspberry Pi
    ======
        1. Run the command: sudo raspi-config
        2. Go to: Interface Options → I2C → Enable
        3. After enabling, reboot the Raspberry Pi: sudo reboot

    Step 2: Build the Kernel Driver
    ======
        1. In your project directory, run: make
        2. Check if the device at address 0x77: dmesg | grep -i 'i2c\|bmp'
        3. Load the BMP180 driver module: sudo insmod ./bmp180_ioctl.ko
        4. Check the kernel log to ensure the driver has loaded successfully:
            dmesg | grep bmp180

    Step 3: Use Device Tree Overlay (Optional)
    ======
        1. Compile the device tree overlay: 
            sudo dtc -@ -I dts -O dtb bmp180_overlay.dts -o bmp180_overlay.dtbo
        2. Move the .dtbo file to the overlays directory:
            sudo cp bmp180_overlay.dtbo /boot/overlays/
        3. Edit the boot configuration file:
            sudo nano /boot/config.txt
        4. Add the following line at the end of the file:
            dtoverlay=bmp180_overlay
        5. Reboot the Raspberry Pi:
            sudo reboot

    Step 4: Compile file test_bmp180:    
    ======
        gcc test_bmp180.c -o run

    Once these steps are completed, the BMP180 sensor should be properly integrated
    with the Raspberry Pi, and you can begin working with the driver and user-space application.

Driver Debugging:
================
    * To Clean: 
        make clean
    * To Remove Module: 
        sudo rmmod bmp180_ioctl
