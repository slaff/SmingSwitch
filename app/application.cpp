#include <SmingCore.h>
#include <Libraries/DHTesp/DHTesp.h>

Timer humidityTimer;
Timer procTimer;
Timer switchTimer;
bool state = true;
bool switchedAllowed = true;
float humidity = 0;
float temperature = 0;


bool running = false;

DHTesp dht;

void onNtpReceive(NtpClient& client, time_t timestamp);
NtpClient ntpClient(onNtpReceive);

void blink()
{
	digitalWrite(LED_PIN, state);
	state = !state;
}

void startSwitch()
{
	if(running) {
		return;
	}

	if(humidity > 60 && switchedAllowed) {
		digitalWrite(SWITCH_PIN, HIGH);
	}
}

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

void onConnected(IpAddress ip, IpAddress netmask, IpAddress gateway)
{
	procTimer.stop();

	// start NTP to get the current time and date...
	ntpClient.requestTime();
}

void onDisconnected(String ssid, const MacAddress& bssid, WifiDisconnectReason reason)
{
	procTimer.start();

	if(static_cast<uint8_t>(reason) > 200) {
		WifiStation.smartConfigStart(SCT_EspTouch, onSmartConfig);
	}
}

void onNtpReceive(NtpClient& client, time_t timestamp)
{
	SystemClock.setTime(timestamp, eTZ_UTC); //System timezone is LOCAL so to set it from UTC we specify TZ
	debug_d("Time synchronized: %s", SystemClock.getSystemTimeString().c_str());
	DateTime today(SystemClock.now());
	switchedAllowed = true;
	// check what is the time -> if earlier than 7:00 and later than 21:00 -> don't start the switch (attached fan) yet
	if(today.Hour < 7 && today.Hour > 20) {
		switchedAllowed = false;
	}
}

void onButtonChange()
{
	// TODO: if the button is pressed for more than 7 seconds then start the smart config process
	if(0) {
		onDisconnected(nullptr, MacAddress(), WIFI_DISCONNECT_REASON_NO_AP_FOUND);
	}
}

void checkHumidity()
{
	TempAndHumidity th = dht.getTempAndHumidity();
	humidity = th.humidity;
	temperature = th.temperature;

	float dewPoint = dht.computeDewPoint(temperature, humidity);
	if(dewPoint > 17.0f) {
		startSwitch();
	}
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	// Start detecting if the button was pressed
	attachInterrupt(BUTTON_PIN, onButtonChange, CHANGE);
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	// Start the humidity timer
	dht.setup(DHT_PIN, DHTesp::DHT22);
	humidityTimer.initializeMs<30 * 1000>(checkHumidity).start();

	// Prepare the control of the switch pin
	pinMode(SWITCH_PIN, INPUT);
	switchTimer.initializeMs<60000>(startSwitch);

	// Prepare the LED for blinking. It should start blinking when there is no internet connection
	pinMode(LED_PIN, OUTPUT);
	procTimer.initializeMs(1000, blink);

	WifiAccessPoint.enable(false);
	WifiStation.enable(true);

	WifiEvents.onStationGotIP(onConnected);
	WifiEvents.onStationDisconnect(onDisconnected);
}
