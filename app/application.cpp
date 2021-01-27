#include <SmingCore.h>
#include <Libraries/DHTesp/DHTesp.h>
#include <Network/UPnP/DeviceHost.h>
#include <Network/UPnP/ControlPoint.h>
#include <Network/SSDP/Server.h>
#include "SmingSwitch.cpp"
#include "AppDigiHooks.h"

namespace
{
Timer humidityTimer;
Timer procTimer;
Timer relayTimer;
Timer* buttonTimer = nullptr;
bool state = true;
bool relayAllowed = true;

// Public states of the device
float humidity = 0;
float temperature = 0;
bool running = false;

DHTesp dht;
HttpServer server;

bool isRelayOn();
void setRelayState(bool on);

UPnP::schemas_sming_org::SmingSwitch smingSwitch(1, humidity, temperature, setRelayState, isRelayOn);

NtpClient* ntpClient = nullptr;

void blink()
{
	digitalWrite(LED_PIN, state);
	state = !state;
}

bool isRelayOn()
{
	return digitalRead(RELAY_PIN) == LOW;
}

void setRelayState(bool on)
{
	digitalWrite(RELAY_PIN, on ? LOW : HIGH);
}

void startRelay()
{
	if(running) {
		return;
	}

	if(humidity > 60 && relayAllowed) {
		setRelayState(true);
	}
}

#ifdef ENABLE_SMART_CONFIG
bool onSmartConfig(SmartConfigEvent event, const SmartConfigEventInfo& info)
{
	switch(event) {
	case SCE_Wait:
		debug_d("SCE_Wait\n");
		break;
	case SCE_FindChannel:
		debug_d("SCE_FindChannel\n");
		break;
	case SCE_GettingSsid:
		debugf("SCE_GettingSsid, type = %d\n", info.type);
		break;
	case SCE_Link:
		debug_d("SCE_Link\n");
		WifiStation.config(info.ssid, info.password);
		WifiStation.connect();
		break;
	case SCE_LinkOver:
		debug_d("SCE_LinkOver\n");
		WifiStation.smartConfigStop();
		break;
	}

	// Don't do any internal processing
	return false;
}
#endif

int onHttpRequest(HttpServerConnection& connection, HttpRequest& request, HttpResponse& response)
{
	// Pass the request into the UPnP stack
	if(UPnP::deviceHost.onHttpRequest(connection)) {
		return 0;
	}

	// Not a UPnP request. Handle any application-specific pages here

	auto path = request.uri.getRelativePath();
	if(path.length() == 0 || path == F("index.html")) {
		auto stream = UPnP::deviceHost.generateDebugPage(F("Basic UPnP"));
		response.sendDataStream(stream, MIME_HTML);
		return 0;
	}

	Serial.print("Page not found: ");
	Serial.println(request.uri.Path);

	response.code = HTTP_STATUS_NOT_FOUND;
	return 0;
}

void initUPnP()
{
	// Configure our HTTP Server to listen for HTTP requests
	server.listen(80);
	server.paths.setDefault(onHttpRequest);
	server.setBodyParser(MIME_JSON, bodyToStringParser);
	server.setBodyParser(MIME_XML, bodyToStringParser);

	if(!UPnP::deviceHost.begin()) {
		debug_e("UPnP initialisation failed");
		return;
	}

	// Advertise our SmingSwitch device
	UPnP::deviceHost.registerDevice(&smingSwitch);
}

void onNtpReceive(NtpClient& client, time_t timestamp)
{
	SystemClock.setTime(timestamp, eTZ_UTC); //System timezone is LOCAL so to set it from UTC we specify TZ
	debug_d("Time synchronized: %s", SystemClock.getSystemTimeString().c_str());
	DateTime today(timestamp);
	relayAllowed = true;
	// check what is the time -> if earlier than 7:00 and later than 21:00 -> don't start the relay (attached fan) yet
	if(today.Hour < 7 && today.Hour > 20) {
		relayAllowed = false;
	}

	// check the month -> in sommer the fan is not really needed
	if(today.Month > 5 && today.Month < 9) {
		relayAllowed = false;
	}
}

void onConnected(IpAddress ip, IpAddress netmask, IpAddress gateway)
{
	procTimer.stop();

	if(ntpClient == nullptr) {
		ntpClient = new NtpClient(onNtpReceive);
	}

	// start NTP to get the current time and date...
	ntpClient->requestTime();
	initUPnP();
}

void onDisconnected(String ssid, const MacAddress& bssid, WifiDisconnectReason reason)
{
	procTimer.start();

#ifdef ENABLE_SMART_CONFIG
	if(static_cast<uint8_t>(reason) > 200) {
		WifiStation.smartConfigStart(SCT_EspTouch, onSmartConfig);
	}
#endif
}

void IRAM_ATTR startSmartConfig()
{
	onDisconnected(nullptr, MacAddress(), WIFI_DISCONNECT_REASON_NO_AP_FOUND);
	if(buttonTimer != nullptr) {
		buttonTimer->stop();
	}
}

void IRAM_ATTR onButtonChange()
{
	bool on = (digitalRead(BUTTON_PIN) == LOW);
	if(on && buttonTimer == nullptr) {
		buttonTimer = new Timer();
		buttonTimer->initializeMs<BUTTON_PRESSED_WAIT>(startSmartConfig).startOnce();
	} else if(!on && buttonTimer != nullptr) {
		buttonTimer->stop();
		delete buttonTimer;
		buttonTimer = nullptr;
	}
}

void checkHumidity()
{
	TempAndHumidity th = dht.getTempAndHumidity();
	humidity = th.humidity;
	temperature = th.temperature;

	float dewPoint = dht.computeDewPoint(temperature, humidity);
	if(dewPoint > 17.0f) {
		startRelay();
	}
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

#ifdef ARCH_HOST
	setDigitalHooks(&appDigiHooks);
#endif

	// Start detecting if the button was pressed
	attachInterrupt(BUTTON_PIN, onButtonChange, CHANGE);
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	// Start the humidity timer
	dht.setup(DHT_PIN, DHTesp::DHT22);
	humidityTimer.initializeMs<HUMIDITY_FREQ>(checkHumidity).start();

	// Prepare the control of the relay pin
	pinMode(RELAY_PIN, OUTPUT);
	digitalWrite(RELAY_PIN, HIGH);
	relayTimer.initializeMs<RELAY_INITIAL_DELAY>(startRelay).startOnce();

	// Prepare the LED for blinking. It should start blinking when there is no internet connection
	pinMode(LED_PIN, OUTPUT);
	procTimer.initializeMs<BLINK_FREQ>(blink);

	WifiAccessPoint.enable(false);
	WifiStation.enable(true);

	WifiEvents.onStationGotIP(onConnected);
	WifiEvents.onStationDisconnect(onDisconnected);
}
