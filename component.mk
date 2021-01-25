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


# End of User Settings


## Application Component configuration
## Parameters configured here will override default and ENV values
## Uncomment and change examples:

## Add your source directories here separated by space
# COMPONENT_SRCDIRS := app
# COMPONENT_SRCFILES :=
# COMPONENT_INCDIRS := include

## If you require any Libraries list them here
# ARDUINO_LIBRARIES :=

## List the names of any additional Components required for this project
COMPONENT_DEPENDS := DHTesp UPnP-Schema

## Set paths for any GIT submodules your application uses
# COMPONENT_SUBMODULES :=

## Append any targets to be built as dependencies of the project, such as generation of additional binary files
# CUSTOM_TARGETS += 

## Additional object files to be included with the application library
# EXTRA_OBJ :=

## Additional libraries to be linked into the project
# EXTRA_LIBS :=

## Provide any additional compiler flags
# COMPONENT_CFLAGS :=
# COMPONENT_CXXFLAGS :=

## Configure flash parameters (for ESP12-E and other new boards):
SPI_MODE := dio

## SPIFFS options
DISABLE_SPIFFS := 1
# SPIFF_FILES = files


ifneq ($(SMING_ARCH),Host)
ENABLE_SMART_CONFIG = 1
endif
 
APP_CFLAGS := -DLED_PIN=$(LED_PIN) -DRELAY_PIN=$(RELAY_PIN) -DDHT_PIN=$(DHT_PIN) -DBUTTON_PIN=$(BUTTON_PIN) \
              -DBLINK_FREQ=$(BLINK_FREQ) -DHUMIDITY_FREQ=$(HUMIDITY_FREQ) -DRELAY_INITIAL_DELAY=$(RELAY_INITIAL_DELAY) -DBUTTON_PRESSED_WAIT=$(BUTTON_PRESSED_WAIT)