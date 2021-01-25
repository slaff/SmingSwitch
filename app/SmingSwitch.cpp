#include <SmingSwitch.h>

namespace UPnP
{
namespace schemas_sming_org
{
SmingSwitch::SmingSwitch(uint8_t id, float& temperature, float& humidity, RelayChangeCallback changeCallback,
						 RelayStatusCallback statusCallback)
	: SmingSwitch1Template(), id(id), temperature(temperature), humidity(humidity), changeCallback(changeCallback),
	  statusCallback(statusCallback)
{
	addService(new SwitchTh016Service(*this));
}

} // namespace schemas_sming_org
} // namespace UPnP
