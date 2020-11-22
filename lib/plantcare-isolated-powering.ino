/*
	Plantcare isolated powering sketch

	This sketch handles the processes needed to take care of a list of Plantcares through
  this web service. Additionally it allows to work to Arduino UNO with an isolated powering system thanks
  to the module TPL5110 from Adafruit (further information https://www.adafruit.com/product/3435).
  The TPL5110 Power Timer is a stand-alone breakout that will turn any electronics into low-power
  electronics. Due to that it will take care of turning ON/OFF the Arduino UNO using a built in timer
  using the DONE pin.

  The workflow:

  - Request scheduled waterings from Plantcare to enable watering from the requested water pumps.
  - Read wet statuses from sensors locally to be handled by Plantcare.
  - Turn off the Arduino UNO. The time to turn it on is handled via hardware. Due to that
    it should be configured following the TPL5110 documentation.


	The circuit:
	- ESP8266 connected through pins 2 and 3 (RX, TX).
  - Wet sensors x 4 connected through analog pins A0, A1, A2 and A3.
	- Relay optocoupler x 4 connected through digital pins 4, 5, 6, and 7.
  - TPL5110 from Adafruit connected to digital pin 8.

	Created 25/07/2020
	By Luis Miguel Bautista
*/

#include "ArduinoJson.h"
#include "SoftwareSerial.h"
#include "string.h"

// ESP8266 config
#define RX 2
#define TX 3
SoftwareSerial SerialESP8266(RX, TX);

// WLAN config
char *wifi = "**************";
char *pass = "**************";

// Plantcare config
char *token = "**************";

// Pin DONE for TPL5110 to turn off the circuit.
int pinDone = 8;

// Wet sensors config
int wetLow = 550;
int wetHigh = 135;
int wetSensorsLength = 4;
int wetSensorsSwitches[] = {10, 11, 12, 13};
int wetSensors[] = {A0, A1, A2, A3};
char *wetSensorsFields[4] = {"A0", "A1", "A2", "A3"};
float wetSensorValues[4];

// Water pumps config
int waterPumpsLength = 4;
int waterPumps[] = {4, 5, 6, 7};
char *waterPumpsFields[4] = {"IN1", "IN2", "IN3", "IN4"};
long *wateringDurations[4] = {0, 0, 0, 0};

// JSON deserialization
const size_t JSONcapacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + 90;

struct HttpResponse
{
  String httpStatus;
  String body;
};

void setup()
{
  Serial.begin(9600);
  SerialESP8266.begin(9600);

  SerialESP8266.println("AT");
  if (SerialESP8266.find("OK"))
  {
    Serial.println(F("ESP8266 response AT OK"));
  }
  else
  {
    Serial.println(F("ESP8266 response AT Error"));
  }

  SerialESP8266.println(F("AT+CWMODE=1"));
  if (SerialESP8266.find("OK"))
  {
    Serial.println(F("ESP8266 station mode OK"));
  }
  else
  {
    Serial.println(F("ESP8266 station mode Error"));
  }

  SerialESP8266.println(String(F("AT+CWJAP=\"")) + wifi + F("\",\"") + pass + F("\""));
  Serial.println(String(F("ESP8266 is connected to ")) + wifi);

  delay(5000);

  SerialESP8266.println(F("AT+CIPMUX=0"));
  if (SerialESP8266.find("OK"))
  {
    Serial.println(F("ESP8266 multi-connections disabled OK"));
  }
  else
  {
    Serial.println(F("ESP8266 multi-connections disabled Error"));
  }

  // Config and close water pumps
  for (int i = 0; i < waterPumpsLength; i++)
  {
    pinMode(waterPumps[i], OUTPUT);
    CloseWaterPump(i);
  }

  // Config and close wet sensors
  for (int i = 0; i < wetSensorsLength; i++)
  {
    pinMode(wetSensorsSwitches[i], OUTPUT);
    DisableWetSensor(i);
  }

  // Config pin DONE for TPL5110
  pinMode(pinDone, OUTPUT);
  delay(1);
  digitalWrite(pinDone, LOW);
}

void loop()
{
  delay(1);
  CheckWatering();
  delay(1);
  UpdateWetStatus();
  delay(1);
  TurnOff();
}
// Enable wet sensor to read analog output
void EnableWetSensor(int idx) { digitalWrite(wetSensorsSwitches[idx], HIGH); }
// Disable wet sensor to read analog output
void DisableWetSensor(int idx) { digitalWrite(wetSensorsSwitches[idx], LOW); }
// Close water pump to stop wet plantcare
void CloseWaterPump(int idx) { digitalWrite(waterPumps[idx], HIGH); }
// Open water pump to wet plantcare
void OpenWaterPump(int idx) { digitalWrite(waterPumps[idx], LOW); }
// Trigger DONE signal for TPL5110
void TurnOff()
{
  delay(1);
  digitalWrite(pinDone, HIGH);
  delay(1);
  digitalWrite(pinDone, LOW);
  delay(1);
}
// Get the wet values of current sensors and post each of them to Plantcare server
void UpdateWetStatus()
{
  for (int idx = 0; idx < wetSensorsLength; idx++)
  {
    ReadWetSensor(idx);
    delay(1);
    PostWetStatus(idx);
  }
}
// Enable, read, and disable a wet sensor.
void ReadWetSensor(int idx)
{
  EnableWetSensor(idx);
  delay(1);
  int wetSensor = wetSensors[idx];
  wetSensorValues[idx] = analogRead(wetSensor);
  delay(1);
  DisableWetSensor(idx);
}
// Post HTTP request with the wet value to Plantcare server.
void PostWetStatus(int wetSensorIdx)
{
  HttpResponse response;
  SerialESP8266.println(F("AT+CIPSTART=\"TCP\",\"api.yourplantcare.com\",80"));

  if (SerialESP8266.find("OK"))
  {
    Serial.println(F("ESP8266 connected to server"));

    String wetSensorField = wetSensorsFields[wetSensorIdx];
    int wet = map(wetSensorValues[wetSensorIdx], wetLow, wetHigh, 0, 100);

    Serial.print(">>>>> wetSensorField " + wetSensorField + ": ");
    Serial.println(wet);

    String params = String(F("{\"wet_sensor_field\":\"")) + wetSensorField + String(F("\",\"wet\":")) + wet + F("}");
    String request = String(F("POST /v1/arduino/wet_statuses HTTP/1.1\r\n"))
                     + F("Host: api.yourplantcare.com\r\n")
                     + F("Authorization: Token ") + token + F("\r\n")
                     + F("Content-Type: application/json\r\n")
                     + F("Content-Length: ") + params.length() + F("\r\n")
                     + F("Connection: close\r\n\r\n")
                     + params + "\r\n";

    HttpResponse response = SendHttpRequest(request);

    Serial.print(F(" >>>>>> HTTP response "));
    Serial.print(response.httpStatus);
  }
  else
  {
    Serial.println(F("ESP8266 was not able to connect to server"));
  }
  Serial.println();
  Serial.println();
}
// Check pending waterings from Plantcare and enable the involved water pumps.
void CheckWatering()
{
  GetWaterPumpStatus();
  for (int idx = 0; idx < waterPumpsLength; idx++)
  {
    if (wateringDurations[idx] != 0)
    {
      OpenWaterPump(idx);
      delay(wateringDurations[idx]);
      CloseWaterPump(idx);
      delay(1);
      EnableWetSensor(idx);
      delay(1);
      wateringDurations[idx] = 0;
    }
  }
}
// Get pending watering from Plantcare server.
void GetWaterPumpStatus()
{
  SerialESP8266.println(F("AT+CIPSTART=\"TCP\",\"api.yourplantcare.com\",80"));
  if (SerialESP8266.find("OK") || SerialESP8266.find("ALREADY CONNECTED"))
  {
    Serial.println(F("ESP8266 connected to server"));
    String request = String(F("POST /v1/arduino/watering_consumers/ HTTP/1.1\r\n"))
                     + F("Host: api.yourplantcare.com\r\n")
                     + F("Connection: close\r\n")
                     + F("Authorization: Token ") + token + F("\r\n\r\n");

    HttpResponse response = SendHttpRequest(request);

    if (response.httpStatus == "200" && response.body != NULL)
    {
      DynamicJsonDocument jsonResponse(JSONcapacity);
      char strBody[response.body.length() + 1];

      strcpy(strBody, response.body.c_str());
      deserializeJson(jsonResponse, strBody);
      serializeJsonPretty(jsonResponse, Serial);

      for (int i = 0; i < waterPumpsLength; i++)
      {
        char *watePumpField = waterPumpsFields[i];
        JsonObject watering = jsonResponse[waterPumpsFields[i]];

        if (!watering.isNull())
        {
          const char *wateringDuration = watering["duration_in_seconds"];
          Serial.println(F("Setting watering"));
          Serial.println(wateringDuration);

          wateringDurations[i] = atol(wateringDuration) * 1000L;
        }
      }
    }
  }
  else
  {
    Serial.println(F("ESP8266 was not able to connect to server"));
  }
  Serial.println();
  Serial.println();
}
// Eval HTTP response
HttpResponse GetHttpResponse()
{
  struct HttpResponse response = {};
  String line;

  while (SerialESP8266.available())
  {
    line = SerialESP8266.readStringUntil('\r');
    if (line.indexOf(F("HTTP/1.1 200 OK")) >= 0)
    {
      response.httpStatus = "200";
    }
    if (line.indexOf(F("HTTP/1.1 201 Created")) >= 0)
    {
      response.httpStatus = "201";
    }
    if (line.indexOf(F("HTTP/1.1 404 Not Found")) >= 0)
    {
      response.httpStatus = "404";
    }
    if (line.indexOf(F("HTTP/1.1 401 Unauthorized")) >= 0)
    {
      response.httpStatus = "401";
    }
    if (line.indexOf(F("HTTP/1.1 500 Internal Server Error")) >= 0)
    {
      response.httpStatus = "500";
    }
    if (line.indexOf(F("{\"")) >= 0)
    {
      response.body = String(line);
    }
  }

  return response;
}
// Send HTTP request via AT commands.
HttpResponse SendHttpRequest(String request)
{
  HttpResponse response;
  SerialESP8266.print("AT+CIPSEND=");
  SerialESP8266.println(request.length());

  // We got the OK from ESP to send the request
  if (SerialESP8266.find(">"))
  {
    Serial.println(F("Sending HTTP request"));
    SerialESP8266.println(request);
    if (SerialESP8266.find("SEND OK"))
    {
      Serial.println(F("HTTP request sent:"));
      Serial.println(request);
      Serial.println(F("Waiting HTTP response"));
      response = GetHttpResponse();
    }
    else
    {
      Serial.println(F("HTTP request could't be sent"));
    }
  }

  return response;
}
