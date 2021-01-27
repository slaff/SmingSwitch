#include <AppDigiHooks.h>
#include <pins_arduino.h>
#include <math.h>

AppDigiHooks appDigiHooks;

void AppDigiHooks::digitalWrite(uint16_t pin, uint8_t val)
{
	if(pin > MAX_PINS - 1) {
		return;
	}
	pins[pin] = val;
}

uint8_t AppDigiHooks::digitalRead(uint16_t pin, uint8_t mode)
{
	if(pin > MAX_PINS - 1) {
		return 0;
	}
	return pins[pin];
}
