# ESP8266 sdk package home directory
ESP_HOME ?= c:/Espressif

# Default COM port
COM_PORT	 ?= COM59

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= $(ESP_HOME)/ESP8266_SDK
SDK_TOOLS	 ?= $(ESP_HOME)/utils

# Other tools mappings
ESPTOOL		 ?= $(SDK_TOOLS)/esptool.exe
#KILL_TERM    ?= taskkill.exe -f -im Termite.exe || exit 0
GET_FILESIZE ?= stat --printf="%s"
#TERMINAL     ?= start $(SDK_TOOLS)/Termite.exe $(COM_PORT) $(COM_SPEED_SERIAL)
MEMANALYZER  ?= $(SDK_TOOLS)/memanalyzer.exe $(OBJDUMP).exe


SPI_MODE ?= dio
SPI_SIZE ?= 4M
SPI_SPEED ?= 80


WIFI_SSID=ATomAP
WIFI_PWD=Smaster1

VERBOSE=1