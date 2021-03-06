/*
 * HardwareServoChannel.h
 *
 *  Created on: 27.11.2015
 *      Author: johndoe
 */

#ifndef APP_HARDWARESERVOCHANNEL_H_
#define APP_HARDWARESERVOCHANNEL_H_

#include "../../SmingCore/SmingCore.h"

#define DEFAULTMAXVALUE 2300
#define DEFAULTMINVALUE 700


class HardwareServoChannel
{
public:
	HardwareServoChannel();
	virtual ~HardwareServoChannel();

	bool attach(uint8 pin);
	bool detach();

	bool setDegree(int8 value);

	bool setMaxValue(uint32 maxValue = DEFAULTMAXVALUE);
	bool setMinValue(uint32 minValue = DEFAULTMINVALUE);
	uint32 getMaxValue() const { return maxValue; }
	uint32 getMinValue() const { return minValue; }

	uint32 getValue() const;
	bool setValue(uint32 value);

	uint8 getPin() const { return pin; }

private:
	void plausValue();
	uint8 pin;
	uint32 value;
	uint32 minValue;
	uint32 maxValue;
};

#endif /* APP_HARDWARESERVOCHANNEL_H_ */
