
# Baretag Tool-Tracker

Item locating and theft prevention system built around the DWM3001C using Ultra Wideband, BLE, and LoRa technologies.


## Hardware

Qorvo DWM3001C - UWB & BLE SoC using nRF52833 and Qorvo DWM3000  

Reyax RYLR998 - LoRa  

3.7V 290mAh LiPo - Tag Battery  

3.7 2200mAh LiIon - Anchor Battery  

CP2102 Serial Converter - LoRa to Serial  

Raspberry Pi 4B - Host for Lora processing  

Dell NUC - Host for servers  

Tag-Connect Edge-Connect Programmer - Programming Tag PCBs  

SEGGER JLink Edu Mini - Programming Anchor PCBs  

## Software

SEGGER Embedded Studio - Build environement  

nRF5 SDK - Nordic libraries for nRF52833  

Xcode - UI/UX  

Debian Linux - Light distro for NUC  

J-Flash Lite - Flashing firmware to DWM3001C  

SQLite - Database management  

Electron - App development  

KiCad - PCB design   
 
# Setup

Run the shell script that we have yet to make in order to install project dependencies
This will create a working project development environment


