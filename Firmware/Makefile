# Makefile for building and flashing code to the DWM3001C
# Requirements:
#   -> nrf5_sdk_17.1.0: https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/sdks/nrf5/binaries/nrf5_sdk_17.1.0_ddde560.zip
#	-> Segger Embedded Studio ARM: https://www.segger.com/downloads/embedded-studio/Setup_EmbeddedStudio_ARM_v542a_linux_x64.tar.gz
#	-> nrf-command-line-tools: https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-23-2/nrf-command-line-tools_10.23.2_amd64.deb
#	-> JLink_Linux: https://www.segger.com/downloads/jlink/JLink_Linux_V792n_x86_64.tgz
#
# Install download and unzip all these above dependencies. Then copy them to these locations:
#	-> nrf5_sdk_17.1.0: /usr/local/
#	-> Segger Embedded Studio ARM: /usr/local
#	-> nrf-command-line-tools: /opt/
#	-> JLink_Linux: /opt/SEGGER/JLink_V794x/
# 
# Some of these may already be in the correct locations, check before copying.

# Run: sudo make build
# Builds an executable that can be found in: Firmware/Output/Common/Exe
build:
	/usr/local/segger_embedded_studio_V5.42a/bin/emBuild -config "Common" dw3000_api.emProject

# Run: make clean
clean:
	/usr/local/segger_embedded_studio_V5.42a/bin/emBuild -config "Common" -clean dw3000_api.emProject

# Run: make flash
# Used for flashing hex to DWM3001CDK - make sure micro USB is plugged into the lower port on the DWM3001CDK
flash:
	nrfjprog --force -f nrf52 --program "$$(pwd)"/Output/Common/Exe/dw3000_api.hex --sectorerase --verify

#Run: make stream-debug-logs
stream-debug-logs:
	echo "Run this command to view debug logs: tail -f Output/debug-log.txt"
	/opt/SEGGER/JLink_V794e/JLinkRTTLogger -Device NRF52833_XXAA -if SWD -Speed 4000 -RTTChannel 0 "$$(pwd)"/Output/debug-log.txt