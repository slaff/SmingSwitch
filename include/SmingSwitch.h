#include <Network/UPnP/schemas-sming-org/ClassGroup.h>

namespace UPnP
{
namespace schemas_sming_org
{
class SmingSwitch : public device::SmingSwitch1Template<SmingSwitch>
{
public:
	using PowerChangeCallback = Delegate<void(bool state)>;
	using PowerStatusCallback = Delegate<bool()>;

	SmingSwitch(uint8_t id, float& temperature, float& humidity, PowerChangeCallback changeCallback,
				PowerStatusCallback statusCallback)
		: id(id), temperature(temperature), humidity(humidity), changeCallback(changeCallback),
		  statusCallback(statusCallback)
	{
	}

	String getField(Field desc) const override
	{
		switch(desc) {
		case Field::UDN:
			// This is the unique id of the device
			return F("uuid:68317e07-d356-455a-813b-d23f2556354a");
			//		case Field::serialNumber:
			//			return F("12345678");
		default:
			return Device::getField(desc);
		}
	}

	// Ensure URL is unique if there are multiple devices
	String getUrlBasePath() const override
	{
		String path = Device::getUrlBasePath();
		path += '/';
		path += id;
		return path;
	}

	float getTemperature()
	{
		return temperature;
	}

	float getHumidity()
	{
		return temperature;
	}

	bool getPower()
	{
		return statusCallback();
	}

	void setPower(bool state)
	{
		return changeCallback(state);
	}

private:
	uint8_t id;
	float temperature;
	float humidity;
	PowerChangeCallback changeCallback;
	PowerStatusCallback statusCallback;
};

class SwitchTh016Service : public service::SwitchTh0161Template<SwitchTh016Service>
{
public:
	using SwitchTh0161Template::SwitchTh0161Template;

	SmingSwitch& getDevice()
	{
		return reinterpret_cast<SmingSwitch&>(device());
	}

	String getField(Field desc) const override
	{
		switch(desc) {
		case Field::serviceId:
			return F("urn:Belkin:serviceId:metainfo1");
		default:
			return Service::getField(desc);
		}
	}

	Error getTemperature(GetTemperature::Response response)
	{
		response.setTemperatureValue(getDevice().getTemperature());
		return Error::Success;
	}

	Error getHumidity(GetHumidity::Response response)
	{
		response.setHumidityValue(getDevice().getHumidity());
		return Error::Success;
	}

	Error getPower(GetPower::Response response)
	{
		response.setRetPowerStatusValue(getDevice().getPower());
		return Error::Success;
	}

	Error setPower(bool state, SetPower::Response response)
	{
		getDevice().setPower(state);
		return Error::Success;
	}
};

} // namespace schemas_sming_org
} // namespace UPnP
