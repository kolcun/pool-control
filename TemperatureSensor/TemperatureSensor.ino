#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "credentials.h"

#define ONE_WIRE_BUS D2

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

char* overwatchTopic = "kolcun/temperature/overwatch";
char* controlTopic = "kolcun/temperature";
char* stateTopicC = "kolcun/temperature/state/C";
char* stateTopicF = "kolcun/temperature/state/F";
char onlineMessage[50] = "Temperature Sensor Online";
char* server = MQTT_SERVER;
char* mqttMessage;
int controllerId;
boolean ledEnabled = true;
char address[4];
char clientId[25] = "temperature-1";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);

  connectToWifi();
  sensors.begin();
  Serial.println(clientId);

  pubSubClient.setServer(server, 1883);
  //pubSubClient.setCallback(mqttCallback);

  if (!pubSubClient.connected()) {
    reconnect();
  }

  blink3Times();
}

void reconnect() {
  while (!pubSubClient.connected()) {
    if (pubSubClient.connect(clientId, MQTT_USER, MQTT_PASSWD)) {
      Serial.println("Connected to MQTT broker");
      pubSubClient.publish(overwatchTopic, onlineMessage);
      Serial.print("sub to: '");
      Serial.print(controlTopic);
      Serial.println("'");
      //if (!pubSubClient.subscribe(controlTopic, 1)) {
      // Serial.println("MQTT: unable to subscribe");
      //}
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

}
void loop() {
  if (!pubSubClient.connected()) {
    reconnect();
  }
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print(" Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  float currentTempC = sensors.getTempCByIndex(0);
  float currentTempF = sensors.getTempFByIndex(0);
  Serial.print("Temperature C is: ");
  Serial.println(currentTempC);
  Serial.print("Temperature C is: ");
  Serial.println(currentTempF);

  String tempC;
  tempC += currentTempC;
  String tempF;
  tempF += currentTempF;
  pubSubClient.publish(stateTopicC, (char *) tempC.c_str());
  pubSubClient.publish(stateTopicF, (char *) tempF.c_str());
  // You can have more than one IC on the same bus.
  // 0 refers to the first IC on the wire
  delay(1000);

  pubSubClient.loop();

}

void connectToWifi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void blink3Times() {
  for (int i = 0; i < 3; i++) {
    turnOnLed();
    delay(100);
    turnOffLed();
    delay(100);
  }
  delay(2000);

}
void turnOnLed() {
  Serial.println("led on");
  digitalWrite(LED_BUILTIN, LOW);
}

void turnOffLed() {
  Serial.println("led off");
  digitalWrite(LED_BUILTIN, HIGH);
}
