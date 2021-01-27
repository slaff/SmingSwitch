#pragma once

#include <DigitalHooks.h>

#ifndef MAX_PINS
#define MAX_PINS 32
#endif

class AppDigiHooks : public DigitalHooks
{
public:
	void digitalWrite(uint16_t pin, uint8_t val) override;

	uint8_t digitalRead(uint16_t pin, uint8_t mode) override;

	virtual ~AppDigiHooks()
	{
	}

private:
	uint8_t pins[MAX_PINS]{0};
};

extern AppDigiHooks appDigiHooks;
