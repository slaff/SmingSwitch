# User Settings (See also: https://templates.blakadder.com/sonoff_TH.html)

# PINS - these should match Sonoff TH16 models up to 2020
LED_PIN ?= 13
RELAY_PIN ?= 12
DHT_PIN ?= 14
BUTTON_PIN ?= 0

# Frequencies (in ms)
BLINK_FREQ ?= 1000
HUMIDITY_FREQ ?= 30000
RELAY_INITIAL_DELAY ?= 60000
BUTTON_PRESSED_WAIT ?= 70000

# GPIO setins
MAX_PINS ?= 32


## [Application id and version] ## 
# Application id
APP_ID ?= "switch"

# Application version: string containing only the major and minor version separated by comma
APP_VERSION := "0.2"
# Application patch version: integer containing only the patch version
APP_VERSION_PATCH := 0

## [TLS/SSL settings ] ##
# Uncomment the line below to start using SSL
CONFIG_VARS += ENABLE_SSL 
# ENABLE_SSL := Bearssl

# Set this to one if the remote firmware server requires client certificate
# This option is in effect only when ENABLE_SSL is set
CONFIG_VARS += ENABLE_CLIENT_CERTIFICATE
ENABLE_CLIENT_CERTIFICATE ?= 0

## [ Firmware Update Server ] ## 
CONFIG_VARS += MQTT_URL
ifeq ($(MQTT_URL),)
    MQTT_URL := "mqtt://attachix.com:1883"
    ifdef ENABLE_SSL
    	ifneq ($(ENABLE_CLIENT_CERTIFICATE),0)
    		MQTT_URL := "mqtts://test.mosquitto.org:8884"
    	else
    		MQTT_URL := "mqtts://test.mosquitto.org:8883"
    	endif
    endif
endif

# This variable contains the SHA1 fingerprint of the SSL certificate of the MQTT server. 
# It is used for certificate pinning. Make sure to change it whenever changing the MQTT_URL
CONFIG_VARS += MQTT_FINGERPRINT_SHA1
MQTT_FINGERPRINT_SHA1 := "0xEE,0xBC,0x4B,0xF8,0x57,0xE3,0xD3,0xE4,0x07,0x54,0x23,0x1E,0xF0,0xC8,0xA1,0x56,0xE0,0xD3,0x1A,0x1C"

CONFIG_VARS += ENABLE_OTA_ADVANCED
ENABLE_OTA_ADVANCED ?= 0

## End of user configurable settings. Don't change anything below this line



# End of User Settings

COMPONENT_DEPENDS := DHTesp UPnP-Schema OtaUpgradeMqtt

CONFIG_VARS += ENABLE_SMART_CONFIG
ifneq ($(SMING_ARCH),Host)
ENABLE_SMART_CONFIG = 1
else
COMPONENT_SRCDIRS += app/Host
endif
 
APP_CFLAGS := -DLED_PIN=$(LED_PIN) \
              -DRELAY_PIN=$(RELAY_PIN) \
              -DDHT_PIN=$(DHT_PIN) \
              -DBUTTON_PIN=$(BUTTON_PIN) \
              -DBLINK_FREQ=$(BLINK_FREQ) \
              -DHUMIDITY_FREQ=$(HUMIDITY_FREQ) \
              -DRELAY_INITIAL_DELAY=$(RELAY_INITIAL_DELAY) \
              -DBUTTON_PRESSED_WAIT=$(BUTTON_PRESSED_WAIT) \
              -DMAX_PINS=$(MAX_PINS) \
              -DMQTT_URL="\"$(MQTT_URL)"\"                \
              -DMQTT_FINGERPRINT_SHA1=$(MQTT_FINGERPRINT_SHA1)   \
              -DAPP_ID="\"$(APP_ID)"\"                    \
              -DENABLE_CLIENT_CERTIFICATE=$(ENABLE_CLIENT_CERTIFICATE)  \
              -DENABLE_OTA_ADVANCED=$(ENABLE_OTA_ADVANCED)
            
## use rboot build mode
RBOOT_ENABLED := 1

## Use standard hardware config with two ROM slots and two SPIFFS partitions
HWCONFIG := two-roms

ifneq ($(APP_VERSION),)
	APP_CFLAGS += -DAPP_VERSION="\"$(APP_VERSION)"\"       \
		      -DAPP_VERSION_PATCH=$(APP_VERSION_PATCH)
endif
