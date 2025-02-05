# run SEGGER Embedded Studio's build system and output the results to ./Output/Common/Exe
build:
	"$$(find /usr/* -type d -name "segger_embedded_studio_for_arm_5.42a")"/bin/emBuild -config "Common" "$$(pwd)"/dw3000_api.emProject

# remove all build outputs from the project (you may want to run this if you run into issues with stale interfaces)
clean: 
	"$$(find /usr/* -type d -name "segger_embedded_studio_for_arm_5.42a")"/bin/emBuild -config "Common" -clean "$$(pwd)"/dw3000_api.emProject

flash: build
	nrfjprog --force -f nrf52 --program "$$(pwd)"/Output/Common/Exe/dw3000_api.hex --sectorerase --verify

stream-debug-logs:
	echo "Run this command to view debug logs: tail -f Output/debug-log.txt"
	/opt/SEGGER/JLink_V794i/JLinkRTTLogger -Device NRF52833_XXAA -if SWD -Speed 4000 -RTTChannel 0 "$$(pwd)"/Output/debug-log.txt