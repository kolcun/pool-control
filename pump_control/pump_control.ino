
//TODO
// as part of boot, need to get the correct values for these from openHab, so current state is valid
// since a mqtt status message is sent indicating the controller is online
// we can leave it up to openhab to send two responses, indicating the current state of the light and heater
// openhab will send the standard commmand message, not status. 
// for example message to topic:kolcun/outdoor/pool/heater message:on
//
// TODO handle heater on state / pump speed minmum details.
// Heater details
//      //openhab will
//      //  if heater on ( check with insteon state )
//      // - tell arduino via bus - start blinking light
//      // - turn heater off (insteon)
//      // - wait for heater to flush (5 minutes)
//      // - tell arduino via bus to set speed to chlorine only
//      // - tell arduino via bus - light off
//
//      //  if heater off (check with insteon state )
//      // - tell arduino via bus - start blinking light
//      // - tell arduino via bus - set pump speed to heater speed
//      // - wait for pump to get to speed (5-10s);
//      // - tell insteon to turn heater on
//      // - tell arduino via bus - solid light
//
// are the light and heater buttons reflecting the state of the system?
// no - they do not turn on, if the light/heater are turned on via openhab, or manually.
// need to sync these buttons with the state of the system - be able to tell the panel to turn on 
// lights with no action.
#include <Ethernet.h>
#include <PubSubClient.h>
#include <MatrixOrbitali2c.h>
#include <Event.h>
#include <Timer.h>
#include <OneButton.h>
#include "credentials.h"

Timer t;
MatrixOrbitali2c lcd(0x2E);

#define latchPin 2
#define clockPin 4
#define dataPin 3

#define tempPin A3

#define RELAYA 5
#define RELAYB 6

#define BUTTON_SPEED1 A0
#define BUTTON_SPEED2 A1
#define BUTTON_SPEED3 A2
#define BUTTON_SPEED4 9
#define BUTTON_HEATER 7
#define BUTTON_POOL_LIGHT 8

OneButton buttonSpeed1(BUTTON_SPEED1, true);
OneButton buttonSpeed2(BUTTON_SPEED2, true);
OneButton buttonSpeed3(BUTTON_SPEED3, true);
OneButton buttonSpeed4(BUTTON_SPEED4, true);
OneButton buttonHeater(BUTTON_HEATER, true);
OneButton buttonPoolLight(BUTTON_POOL_LIGHT, true);

const char lightTopic[27] = "kolcun/outdoor/pool/light";
const char heaterTopic[28] = "kolcun/outdoor/pool/heater";
const char pumpTopic[26] = "kolcun/outdoor/pool/pump";
const char lightStatusTopic[33] = "kolcun/outdoor/pool/light/status";
const char heaterStatusTopic[34] = "kolcun/outdoor/pool/heater/status";
const char pumpStatusTopic[32]  = "kolcun/outdoor/pool/pump/status";
const char controllerTopic[32] = "kolcun/outdoor/pool/controller";
char* mqttMessage;

char tempResult[5];
byte leds = 0;
int currentSpeed = 1;
boolean heaterActive = false;
boolean poolLightActive = false;
boolean lcdOn = true;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x04
};
//local on pi
//IPAddress mqttServer(192, 168, 0, 117);
//amazon

char* mqttServer = MQTT_SERVER;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient ethClient;
PubSubClient pubSubClient(ethClient);

void setup() {
  Serial.begin(9600);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  //start with all LEDs off
  leds = 0;
  updateShiftRegister();
  turnLcdOn();
  lcd.print("Booting\n");

  buttonSpeed1.attachClick(callbackSpeed1);
  buttonSpeed2.attachClick(callbackSpeed2);
  buttonSpeed3.attachClick(callbackSpeed3);
  buttonSpeed4.attachClick(callbackSpeed4);
  buttonHeater.attachClick(callbackHeater);
  buttonPoolLight.attachClick(callbackPoolLight);
  buttonHeater.attachDoubleClick(heaterDoubleClick);

  pinMode(RELAYA, OUTPUT);
  pinMode(RELAYB, OUTPUT);

  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, HIGH);

  pubSubClient.setServer(mqttServer, 1883);
  pubSubClient.setCallback(mqttCallback);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    lcdFatalMessage("Eth DHCP failure");
  }
  // print your local IP address:
  printIPAddress();

  if (!pubSubClient.connected()) {
    reconnectMqtt();
  }

  activateSpeedLevel1();

  Serial.println(F("Ready"));
  lcd.print(F("Boot Complete"));
  lcd.noAutoScroll();
  lcd.clear();
  refreshLcd();
}

void loop() {

  // speeds
  // Speed 1 = Timer 1 = 0 RPM (OFF);
  // Speed 2 = Timer 2 = 1300 RPM (minimum for chlorine)
  // Speed 3 = Timer 3 = 1750 RPM (minimum for Heater)
  // Speed 4 = Timer 4 = 3240 RPM (full speed for vacuum, slide)

  if (!pubSubClient.connected()) {
    reconnectMqtt();
  }

  buttonSpeed1.tick();
  buttonSpeed2.tick();
  buttonSpeed3.tick();
  buttonSpeed4.tick();
  buttonHeater.tick();
  buttonPoolLight.tick();

  pubSubClient.loop();
  t.update();
}

void heaterDoubleClick() {
  displayTemperature();
}

void callbackSpeed1() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  activateSpeedLevel1();
}

void callbackSpeed2() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  activateSpeedLevel2();
}

void callbackSpeed3() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  activateSpeedLevel3();
}

void callbackSpeed4() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  activateSpeedLevel4();
}
void callbackHeater() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  if (heaterActive) {
    setHeaterOff();
  } else {
    setHeaterOn();
  }
}

void setHeaterOn() {
  heaterActive = true;
  //  activateSpeedLevel3();
  bitSet(leds, 4);
  updateShiftRegister();
  publishHeaterStatus();
  refreshLcd();
}

void setHeaterOff() {
  heaterActive = false;
  bitClear(leds, 4);
  updateShiftRegister();
  publishHeaterStatus();
  refreshLcd();
}

void callbackPoolLight() {
  if (!lcdOn) {
    turnLcdOn();
    return;
  }
  if (poolLightActive) {
    setPoolLightOff();
  } else {
    setPoolLightOn();
  }
}

void setPoolLightOn() {
  poolLightActive = true;
  bitSet(leds, 5);
  updateShiftRegister();
  publishPoolLightStatus();
  refreshLcd();
}

void setPoolLightOff() {
  poolLightActive = false;
  bitClear(leds, 5);
  updateShiftRegister();
  publishPoolLightStatus();
  refreshLcd();
}

void activateSpeedLevel1() {
  bitSet(leds, 0);
  bitClear(leds, 1);
  bitClear(leds, 2);
  bitClear(leds, 3);
  updateShiftRegister();
  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, HIGH);
  //  pubSubClient.publish("poolcontrol","Button A pressed");
  currentSpeed = 1;
  publishPumpStatus();
  refreshLcd();
}

void activateSpeedLevel2() {
  bitClear(leds, 0);
  bitSet(leds, 1);
  bitClear(leds, 2);
  bitClear(leds, 3);
  updateShiftRegister();
  digitalWrite(RELAYA, LOW);
  digitalWrite(RELAYB, HIGH);
  //  pubSubClient.publish("poolcontrol","Button B pressed");
  currentSpeed = 2;
  publishPumpStatus();
  refreshLcd();
}

void activateSpeedLevel3() {
  bitClear(leds, 0);
  bitClear(leds, 1);
  bitSet(leds, 2);
  bitClear(leds, 3);
  updateShiftRegister();
  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, LOW);
  //  pubSubClient.publish("poolcontrol","Button C pressed");
  currentSpeed = 3;
  publishPumpStatus();
  refreshLcd();
}

void activateSpeedLevel4() {
  bitClear(leds, 0);
  bitClear(leds, 1);
  bitClear(leds, 2);
  bitSet(leds, 3);
  updateShiftRegister();
  digitalWrite(RELAYA, LOW);
  digitalWrite(RELAYB, LOW);
  //  pubSubClient.publish("poolcontrol","Button D pressed");
  currentSpeed = 4;
  publishPumpStatus();
  refreshLcd();
}

void printIPAddress() {
  lcd.print(F("IP: "));
  Serial.print(F("IP address: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    lcd.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(F("."));
    lcd.print(F("."));
  }
  lcd.print(F("\n"));

  Serial.println();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("MQTT Message Arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  mqttMessage = (char*) payload;

  if (strcmp(topic, controllerTopic) == 0) {
    if (strncmp(mqttMessage, "temp", length) == 0) {
      Serial.println(F("Temp Display"));
      displayTemperature();
    }
  }

  if (strcmp(topic, controllerTopic) == 0) {
    if (strncmp(mqttMessage, "lcdon", length) == 0) {
      turnLcdOn();
    } else if (strncmp(mqttMessage, "lcdoff", length) == 0) {
      turnLcdOff();
    }
  }


  if (strcmp(topic, lightTopic) == 0) {
    Serial.println(F("light topic"));
    if (strncmp(mqttMessage, "status", length) == 0) {
      publishPoolLightStatus();
    } else if (strncmp(mqttMessage, "on", length) == 0) {
      Serial.println(F("light on"));
      setPoolLightOn();
    } else if (strncmp(mqttMessage, "off", length) == 0) {
      Serial.println(F("light off"));
      setPoolLightOff();
    }
  }

  if (strcmp(topic, heaterTopic) == 0) {
    Serial.println(F("heater topic"));
    if (strncmp(mqttMessage, "status", length) == 0) {
      publishHeaterStatus();
    } else if (strncmp(mqttMessage, "on", length) == 0) {
      Serial.println(F("heater on"));
      setHeaterOn();
    } else if (strncmp(mqttMessage, "off", length) == 0) {
      Serial.println(F("heater off"));
      setHeaterOff();
    }
  }

  if (strcmp(topic, pumpTopic) == 0) {
    Serial.println(F("pump topic"));
    if (strncmp(mqttMessage, "status", length) == 0) {
      publishPumpStatus();
    } else if (strncmp(mqttMessage, "Speed1", length) == 0) {
      Serial.println(F("Speed 1"));
      activateSpeedLevel1();
    } else if (strncmp(mqttMessage, "Speed2", length) == 0) {
      Serial.println(F("Speed2"));
      activateSpeedLevel2();
    } else if (strncmp(mqttMessage, "Speed3", length) == 0) {
      Serial.println(F("Speed3"));
      activateSpeedLevel3();
    } else if (strncmp(mqttMessage, "Speed4", length) == 0) {
      Serial.println(F("Speed4"));
      activateSpeedLevel4();
    }
  }
}

void publishPumpStatus() {
  if (currentSpeed == 1) {
    pubSubClient.publish(pumpStatusTopic, "Speed1");
  } else if (currentSpeed == 2) {
    pubSubClient.publish(pumpStatusTopic, "Speed2");
  } else if (currentSpeed == 3) {
    pubSubClient.publish(pumpStatusTopic, "Speed3");
  } else if (currentSpeed == 4) {
    pubSubClient.publish(pumpStatusTopic, "Speed4");
  } else {
    pubSubClient.publish(pumpStatusTopic, "error");
  }

}
void publishHeaterStatus() {
  if (heaterActive) {
    pubSubClient.publish(heaterStatusTopic, "on");
  } else {
    pubSubClient.publish(heaterStatusTopic, "off");
  }

}
void publishPoolLightStatus() {
  if (poolLightActive) {
    pubSubClient.publish(lightStatusTopic, "on");
  } else {
    pubSubClient.publish(lightStatusTopic, "off");
  }
}
void reconnectMqtt() {
  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    if (!lcdOn) {
      turnLcdOn();
    }
    Serial.print(F("Attempting MQTT connection..."));
    lcd.print(F("MQTT: "));
    // Attempt to connect
    if (pubSubClient.connect("arduino-pool-control-client", MQTT_USER, MQTT_PASSWD)) {
      Serial.println(F("connected"));
      lcd.print(F("connected\n"));
      pubSubClient.publish("kolcun/outdoor/pool/controller/status", "controller online");
      if (!pubSubClient.subscribe(lightTopic, 1)) {
        lcdFatalMessage(F("MQTT: unable to subscribe"));
      }
      if (!pubSubClient.subscribe(heaterTopic, 1)) {
        lcdFatalMessage(F("MQTT: unable to subscribe"));
      }
      if (!pubSubClient.subscribe(pumpTopic, 1)) {
        lcdFatalMessage(F("MQTT: unable to subscribe"));
      }
      if (!pubSubClient.subscribe(controllerTopic, 1)) {
        lcdFatalMessage(F("MQTT: unable to subscribe"));
      }
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(pubSubClient.state());
      Serial.println(F(" try again in 5 seconds"));
      lcd.print(F("failed, rc="));
      lcd.print(pubSubClient.state());
      lcd.println(F(" try again in 5s\n"));

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void refreshLcd() {
  if (!lcdOn) {
    return;
  }
  lcd.clear();
  lcd.print(F("Pool Controller "));
  lcd.print(F("\nSpeed: speed "));
  lcd.print(currentSpeed);
  if (heaterActive) {
    lcd.print(F("\nHeater: On"));
  } else {
    lcd.print(F("\nHeater: Off"));
  }
  if (poolLightActive) {
    lcd.print(F("\nLight: On"));
  } else {
    lcd.print(F("\nLight: Off\n"));
  }
}

void updateShiftRegister() {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, leds);
  digitalWrite(latchPin, HIGH);
}

void lcdFatalMessage(const String &s) {
  if (!lcdOn) {
    turnLcdOn();
  }
  Serial.println(s);
  lcd.clear();
  lcd.print(F("--- Fatal Error --- "));
  lcd.print(s);
  //fatal error, loop forever
  for (;;) {}
}

float getDegreesC() {
  return ((analogRead(tempPin) * 0.004882814) - 0.5) * 100.0;
}

void displayTemperature() {
  if (!lcdOn) {
    turnLcdOn();
  }
  lcd.clear();
  lcd.println(F("Enclosure"));
  lcd.print(F("Temperature  "));
  dtostrf(getDegreesC(), 4, 1, tempResult);
  lcd.print(tempResult);
  lcd.print(F("'C"));
  lcd.println();
  delay(5000);
  refreshLcd();
}

void turnLcdOn() {
  lcdOn = true;
  bitClear(leds, 6);
  updateShiftRegister();
  delay(1000);
  initLcd();
  refreshLcd();
  //3 minutes
  t.after(180000, turnLcdOff);
  //15 seconds
  //t.after(15000, turnLcdOff);
}
void turnLcdOff() {
  lcdOn = false;
  bitSet(leds, 6);
  updateShiftRegister();
}
void initLcd() {
  lcd.begin(4, 20);
  lcd.setContrast(200);
  lcd.backlightOn();
  lcd.lineWrap();
  lcd.clear();
  lcd.autoScroll();
}
