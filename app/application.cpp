#include <SmingCore.h>
#include <Libraries/DHTesp/DHTesp.h>
#include <Network/UPnP/DeviceHost.h>
#include <Network/UPnP/ControlPoint.h>
#include <Network/SSDP/Server.h>
#include <Storage/SpiFlash.h>
#include <Ota/Manager.h>
#include <OtaUpgrade/Mqtt/StandardPayloadParser.h>

#if ENABLE_OTA_ADVANCED
#include <OtaUpgrade/Mqtt/AdvancedPayloadParser.h>
#endif

#include "SmingSwitch.cpp"

#ifdef ARCH_HOST
#include "AppDigiHooks.h"
#endif

namespace
{
MqttClient mqtt;

SimpleTimer humidityTimer;
SimpleTimer procTimer;
SimpleTimer relayTimer;
SimpleTimer* buttonTimer = nullptr;
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

void otaUpdate()
{
	if(mqtt.isProcessing()) {
		Serial.println("There is an update in progress. Refusing to start new update.");
		return;
	}

	Serial.println("Checking for a new application firmware...");

	// select rom slot to flash
	auto part = OtaManager.getNextBootPartition();
	if(!part) {
		Serial.println("FAILED: Cannot find application address");
		return;
	}

#ifdef ENABLE_SSL
	mqtt.setSslInitHandler([](Ssl::Session& session) {
		// These fingerprints change very frequently.
		static const Ssl::Fingerprint::Cert::Sha1 sha1Fingerprint PROGMEM = {MQTT_FINGERPRINT_SHA1};

		// Trust certificate only if it matches the SHA1 fingerprint...
		session.validators.pin(sha1Fingerprint);

		// We're using fingerprints, so don't attempt to validate full certificate
		session.options.verifyLater = true;

#if ENABLE_CLIENT_CERTIFICATE
		session.keyCert.assign(privateKeyData, certificateData);
#endif

		// Use all supported cipher suites to make a connection
		session.cipherSuites = &Ssl::CipherSuites::full;
	});
#endif

	mqtt.connect(Url(MQTT_URL), "sming");

#if ENABLE_OTA_ADVANCED
	/*
	 * The advanced parser suppors all firmware upgrades supported by the `OtaUpgrade` library.
	 * `OtaUpgrade` library provides firmware signing, firmware encryption and so on.
	 */
	auto parser = new OtaUpgrade::Mqtt::AdvancedPayloadParser(APP_VERSION_PATCH);
#else
	/*
	 * The command below uses class that stores the firmware directly
	 * using RbootOutputStream on a location provided by us
	 */
	auto parser = new OtaUpgrade::Mqtt::StandardPayloadParser(part, APP_VERSION_PATCH);
#endif

	mqtt.setPayloadParser([parser](MqttPayloadParserState& state, mqtt_message_t* message, const char* buffer,
								   int length) -> int { return parser->parse(state, message, buffer, length); });

	String updateTopic = "a/";
	updateTopic += APP_ID;
	updateTopic += "/u/";
	updateTopic += APP_VERSION;
	debug_d("Subscribing to topic: %s", updateTopic.c_str());
	mqtt.subscribe(updateTopic);
}

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
	static bool otaRunning = false;

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

	if(!otaRunning) {
		otaRunning = true;
		otaUpdate();
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
		buttonTimer = new SimpleTimer();
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
