#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <AccelStepper.h>
#include <time.h>
#include <vector>

#include "Event.h"

#define EN_PIN 27
#define DIR_PIN 19
#define STEP_PIN 18

/* Установите здесь свои SSID и пароль */
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

unsigned int current_steps = 0;

const unsigned int MAX_STEPS = 16000;
String status = "closed";
std::vector<Event> events;

WebServer server(80);

AccelStepper stepper(
	AccelStepper::DRIVER,
	STEP_PIN,
	DIR_PIN);

enum State
{
	OPENING,
	CLOSING,
	NOTHING,
};

State state = NOTHING;

struct tm timeinfo;

void handle_open();
void handle_close();
void handle_set();
void handle_status();
void handle_schedule();
void handle_delete_event();
void handle_event();
void handle_NotFound();
void SendHTML();
void handleFile();

void setup()
{
	Serial.begin(115200);

	// pin settings
	pinMode(EN_PIN, OUTPUT);
	pinMode(STEP_PIN, OUTPUT);
	pinMode(DIR_PIN, OUTPUT);
	digitalWrite(EN_PIN, HIGH);

	// stepper settings
	stepper.setMaxSpeed(2000);
	stepper.setAcceleration(1000);

	Serial.println("TMC2208 ready");

	// mounting fs
	if (!LittleFS.begin())
	{
		Serial.println("LittleFS mount failed");
		return;
	}

	Serial.println("LittleFS mounted");

	Serial.print("Connecting to ");
	Serial.println(ssid);

	// connect to local wi-fi network
	WiFi.begin(ssid, password);

	// check wi-fi is connected to wi-fi network
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.print(".");
		Serial.println(WiFi.status());
	}

	Serial.println("");
	Serial.println("WiFi connected..!");
	Serial.print("Got IP: ");
	Serial.println(WiFi.localIP());

	configTime(
		3 * 3600,
		0,
		"pool.ntp.org",
		"time.nist.gov");

	// setting OTA
	ArduinoOTA.setHostname("smart-roller");
	ArduinoOTA.onStart([]()
					   { Serial.println("OTA Start"); });
	ArduinoOTA.onEnd([]()
					 { Serial.println("\nOTA End"); });
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
						  { Serial.printf("Progress: %u%%\r", (progress * 100) / total); });
	ArduinoOTA.onError([](ota_error_t error)
					   { Serial.printf("Error[%u]\n", error); });
	ArduinoOTA.begin();

	Serial.println("OTA Ready");

	// setting server and handlers
	server.on("/", SendHTML);
	server.on("/open", handle_open);
	server.on("/close", handle_close);
	server.on("/set", handle_set);
	server.on("/status", handle_status);
	server.on("/schedule", handle_schedule);
	server.on("/event/delete", handle_delete_event);
	server.on("/event", handle_event);
	server.onNotFound(handleFile);
	server.begin();

	Serial.println("HTTP server started");
}

void loop()
{
	server.handleClient();
	ArduinoOTA.handle();
	stepper.run();

	static uint32_t lastTimeUpdate = 0;

	if (millis() - lastTimeUpdate > 1000)
	{
		getLocalTime(&timeinfo);
		lastTimeUpdate = millis();
	}

	static int lastMinute = -1;

	events.erase(
		std::remove_if(events.begin(), events.end(),
					   [&](const Event &e)
					   {
						   if (timeinfo.tm_min == lastMinute)
							   return false;
						   if (e.hour == timeinfo.tm_hour && e.minute == timeinfo.tm_min)
						   {
							   digitalWrite(EN_PIN, LOW);
							   int target = e.open ? MAX_STEPS : 0;
							   stepper.moveTo(target);
							   lastMinute = timeinfo.tm_min;
							   return e.once;
						   }

						   return false;
					   }),
		events.end());

	if (stepper.distanceToGo() == 0)
	{
		digitalWrite(EN_PIN, HIGH);
	}
}

void handle_open()
{
	if (stepper.currentPosition() >= MAX_STEPS)
	{
		Serial.println("already opened");
		server.send(
			200,
			"application/json",
			"{\"status\":\"Штора уже открыта\",\"current\":\"10 0\"}");
		return;
	}

	server.send(
		200,
		"application/json",
		"{\"status\":\"Штора открывается\"}");

	digitalWrite(EN_PIN, LOW);
	stepper.moveTo(MAX_STEPS);
}

void handle_close()
{
	if (stepper.currentPosition() <= 0)
	{
		Serial.println("already closed");
		server.send(
			200,
			"application/json",
			"{\"status\":\"Штора уже закрыта\",\"current\":\"0\"}");
		return;
	}

	server.send(
		200,
		"application/json",
		"{\"status\":\"Штора закрывается\"}");

	digitalWrite(EN_PIN, LOW);
	stepper.moveTo(0);
}

void handle_set()
{

	String posStr = server.arg("pos");

	int pos = posStr.toInt();

	digitalWrite(EN_PIN, LOW);

	int target = MAX_STEPS * pos / 100;

	stepper.moveTo(target);

	String json =
		"{\"status\":\"ok\","
		"\"position\":" +
		posStr + "}";

	server.send(200, "application/json", json);
}

void handle_status()
{
	int percent = stepper.currentPosition() * 100 / MAX_STEPS;

	String json =
		"{\"position\":" + String(percent) + "}";

	server.send(200, "application/json", json);
}

void handle_schedule()
{
	String open = server.arg("open");
	String close = server.arg("close");
	String onceStr = server.arg("once");

	Serial.println(open + " " + close);

	bool exists = false;

	Event event;
	event.once = (onceStr == "true");

	if (open.length() > 0)
	{
		uint8_t hour = open.substring(0, 2).toInt();
		uint8_t min = open.substring(3, 5).toInt();
		event.hour = hour;
		event.minute = min;
		event.open = 1;
	}
	else if (close.length() > 0)
	{
		uint8_t hour = close.substring(0, 2).toInt();
		uint8_t min = close.substring(3, 5).toInt();
		event.hour = hour;
		event.minute = min;
		event.open = 0;
	}

	for (Event &e : events)
	{
		if (e.hour == event.hour && e.minute == event.minute)
		{
			exists = true;
			break;
		}
	}

	if (!exists)
	{
		Serial.println("adding");
		events.push_back(event);
		server.send(
			200,
			"application/json",
			"{\"status\":\"Событие добавлено!\"}");
	}
	else
	{
		Serial.println("not adding");
		server.send(
			200,
			"application/json",
			"{\"status\":\"Событие на это время уже существует!\"}");
	}
}

void handle_event()
{
	Serial.println("happening");
	String message = "[";
	bool first = true;

	for (int i = 0; i < events.size(); i++)
	{
		if (!first)
			message += ",";
		first = false;

		char timeBuf[6];
		sprintf(timeBuf, "%02d:%02d", events[i].hour, events[i].minute);

		message += "{";
		message += "\"time\":\"" + String(timeBuf) + "\",";
		message += "\"open\":" + String(events[i].open ? "true" : "false") + ",";
		message += "\"once\":" + String(events[i].once ? "true" : "false");
		message += "}";
	}
	message += "]";

	server.send(200, "application/json", message);
}

void handle_delete_event()
{

	String time = server.arg("time");
	uint8_t hour = time.substring(0, 2).toInt();
	uint8_t min = time.substring(3, 5).toInt();
	Serial.println("del");
	for (int i = 0; i < events.size(); i++)
	{
		if (events[i].hour == hour && events[i].minute == min)
		{
			events.erase(events.begin() + i);
			break;
		}
	}
	server.send(
		200,
		"application/json",
		"{\"status\":\"Событие удалено\"}");
}

void handle_NotFound()
{
	Serial.println("404: " + server.uri());
	server.send(404, "text/plain", "Not found");
}
void SendHTML()
{
	File file = LittleFS.open("/index.html", "r");
	server.streamFile(file, "text/html");
	file.close();
}

void handleFile()
{
	String path = server.uri();

	if (path == "/")
		path = "/index.html";

	File file = LittleFS.open(path, "r");

	if (!file)
	{
		server.send(404, "text/plain", "Not found");
		return;
	}

	String contentType = "text/plain";
	if (path.endsWith(".html"))
		contentType = "text/html";
	else if (path.endsWith(".css"))
		contentType = "text/css";
	else if (path.endsWith(".js"))
		contentType = "application/javascript";
	else if (path.endsWith(".ico"))
		contentType = "image/x-icon";

	server.streamFile(file, contentType);
	file.close();
}

// pio run --target uploadfs