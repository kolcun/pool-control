#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <MatrixOrbitali2c.h>

MatrixOrbitali2c lcd(0x2E);

int latchPin = 2;
int clockPin = 4;
int dataPin = 3;
byte leds = 0;

int RELAYA = 5;
int RELAYB = 6;

//int LED_SPEED1 = 4;
//int LED_SPEED2 = 5;
//int LED_SPEED3 = 6;
//int LED_SPEED4 = 7;
//int LED_HEATER = 8;
//int LED_LIGHT = 9;

int BUTTON_SPEED1 = A0;
int BUTTON_SPEED2 = A1;
int BUTTON_SPEED3 = A2;
int BUTTON_SPEED4 = A3;
int BUTTON_HEATER = 7;//A4;
int BUTTON_LIGHT = 8;//A5;

int currentSpeed = 1;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x04
};
IPAddress mqttServer(192,168,0,117);
//IPAddress mqttServer(192,168,0,136);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient ethClient;
PubSubClient pubSubClient(ethClient);

void setup() {

  Serial.begin(9600);
  
  lcd.begin(4,20);
  lcd.setContrast(180);
  lcd.backlightOn();
  lcd.clear();
  lcd.print("Booting... ");
  
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(clockPin, OUTPUT);
  //start with all LEDs off
  leds = 0;
  updateShiftRegister();
    
//  pinMode(LED_SPEED1, OUTPUT);
//  pinMode(LED_SPEED2, OUTPUT);
//  pinMode(LED_SPEED3, OUTPUT);
//  pinMode(LED_SPEED4, OUTPUT);
//  pinMode(LED_HEATER, OUTPUT);
//  pinMode(LED_LIGHT, OUTPUT);

  pinMode(BUTTON_SPEED1, INPUT_PULLUP);
  pinMode(BUTTON_SPEED2, INPUT_PULLUP);
  pinMode(BUTTON_SPEED3, INPUT_PULLUP);
  pinMode(BUTTON_SPEED4, INPUT_PULLUP);
  pinMode(BUTTON_HEATER, INPUT_PULLUP);
  pinMode(BUTTON_LIGHT, INPUT_PULLUP);
  pinMode(RELAYA, OUTPUT);
  pinMode(RELAYB, OUTPUT);

  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, HIGH);

//  digitalWrite(LED_SPEED1, HIGH); 
//  digitalWrite(LED_SPEED2, LOW); 
//  digitalWrite(LED_SPEED3, LOW); 
//  digitalWrite(LED_SPEED4, LOW);
//  digitalWrite(LED_HEATER, LOW);
//  digitalWrite(LED_LIGHT, LOW);

   // Open serial communications and wait for port to open:


  pubSubClient.setServer(mqttServer, 1883);
  pubSubClient.setCallback(mqttCallback);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for (;;){
      //TODO
//      blinkLED(LED_HEATER,1);
    }
  }
  // print your local IP address:
  printIPAddress();

  delay(1500);
  Serial.println("Ready");
  lcd.print("Complete");
}

void loop() {

  // speeds
  // Speed 1 = Timer 1 = 0 RPM (OFF);
  // Speed 2 = Timer 2 = 1300 RPM (minimum for chlorine)
  // Speed 3 = Timer 3 = 1750 RPM (minimum for Heater)
  // Speed 4 = Timer 4 = 3240 RPM (full speed for vacuum, slide)

  //TODO
  //use the LEDS as boot status, for example, LED1 turns on when it has an IP, LED 2 turns on when MQTT is connected
  //both RED LEDS turn on and blink if there is an error.
  //error contions
  // no ethernet - heater LED blinks forever
  // no MQTT - pool light LED blinks forever

  

  if (!pubSubClient.connected()) {
    reconnect();
  }

  
  if(digitalRead(BUTTON_SPEED1) == LOW && currentSpeed != 1){
    activateSpeedLevel1();
    Serial.println("Speed 1");
  }
  
  if(digitalRead(BUTTON_SPEED2) == LOW && currentSpeed != 2){
    activateSpeedLevel2();
    Serial.println("Speed 2");
//    digitalWrite(LED_HEATER, HIGH);
//    delay(500);
//    digitalWrite(LED_HEATER, LOW);
//    digitalWrite(LED_LIGHT, LOW);
  }

  if(digitalRead(BUTTON_SPEED3) == LOW && currentSpeed != 3){
    activateSpeedLevel3();
    Serial.println("Speed 3");
  }

  if(digitalRead(BUTTON_SPEED4) == LOW && currentSpeed != 4){
    activateSpeedLevel4();
    Serial.println("Speed 4");
  }

  //pool ight button pushed
  if(digitalRead(BUTTON_LIGHT) == LOW){
    Serial.println("heater button pushed");
    activatePoolLight();
  }

  //heater button pushed
  if(digitalRead(BUTTON_HEATER) == LOW){
    Serial.println("heater button pushed");
    activateHeater();

//note - need to remove local control via insteon sense button at heater
//if we use this, there is no way to maintain a consistnet state of the light on the arduino
    
    //openhab will
//  if heater on ( check with insteon state )
    // - tell arduino via bus - start blinking light
    // - turn heater off (insteon)
    // - wait for heater to flush (5 minutes)
    // - tell arduino via bus to set speed to chlorine only
    // - tell arduino via bus - light off

//  if heater off (check with insteon state )
    // - tell arduino via bus - start blinking light
    // - tell arduino via bus - set pump speed to heater speed
    // - wait for pump to get to speed (5-10s);
    // - tell insteon to turn heater on 
    // - tell arduino via bus - solid light
    
    //activateSpeedLevel4();
//    digitalWrite(LED_HEATER, HIGH);
//    delay(500);
//    digitalWrite(LED_HEATER, LOW);
    //activateSpeedLevel1();
  }

  pubSubClient.loop();
                
}

void activateHeater(){
  Serial.println("heater button");
  bitSet(leds,4);
  updateShiftRegister();  
  delay(1500);
  bitClear(leds,4);
  updateShiftRegister();  
}

void activatePoolLight(){
  Serial.println("pool light button");
  bitSet(leds,5);
  updateShiftRegister();  
  delay(1500);
  bitClear(leds,5);
  updateShiftRegister();  
}



void activateSpeedLevel1()
{
    bitSet(leds,0);
  bitClear(leds,1);
  bitClear(leds,2);
  bitClear(leds,3);
  updateShiftRegister();   
//  digitalWrite(LED_SPEED1, HIGH); 
//  digitalWrite(LED_SPEED2, LOW); 
//  digitalWrite(LED_SPEED3, LOW); 
//  digitalWrite(LED_SPEED4, LOW); 
  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, HIGH);
//  pubSubClient.publish("poolcontrol","Button A pressed");
  currentSpeed = 1;
}

void activateSpeedLevel2()
{
  bitClear(leds,0);
  bitSet(leds,1);
  bitClear(leds,2);
  bitClear(leds,3);
  updateShiftRegister();   
//  digitalWrite(LED_SPEED1, LOW); 
//  digitalWrite(LED_SPEED2, HIGH); 
//  digitalWrite(LED_SPEED3, LOW); 
//  digitalWrite(LED_SPEED4, LOW); 
  digitalWrite(RELAYA, LOW);
  digitalWrite(RELAYB, HIGH);
//  pubSubClient.publish("poolcontrol","Button B pressed");  
  currentSpeed = 2;
}

void activateSpeedLevel3()
{
  bitClear(leds,0);
  bitClear(leds,1);
  bitSet(leds,2);
  bitClear(leds,3);
  updateShiftRegister();   
//  digitalWrite(LED_SPEED1, LOW); 
//  digitalWrite(LED_SPEED2, LOW); 
//  digitalWrite(LED_SPEED3, HIGH); 
//  digitalWrite(LED_SPEED4, LOW);
  digitalWrite(RELAYA, HIGH);
  digitalWrite(RELAYB, LOW);
//  pubSubClient.publish("poolcontrol","Button C pressed");  
  currentSpeed = 3;
  }

void activateSpeedLevel4()
{
  bitClear(leds,0);
  bitClear(leds,1);
  bitClear(leds,2);
  bitSet(leds,3);
  updateShiftRegister();   
//  digitalWrite(LED_SPEED1, LOW); 
//  digitalWrite(LED_SPEED2, LOW); 
//  digitalWrite(LED_SPEED3, LOW); 
//  digitalWrite(LED_SPEED4, HIGH); 
  digitalWrite(RELAYA, LOW);
  digitalWrite(RELAYB, LOW);    
//  pubSubClient.publish("poolcontrol","Button D pressed");
  currentSpeed = 4;
}

void printIPAddress()
{
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }

  Serial.println();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  if ((char)payload[0] == '1') {
    activateSpeedLevel1();
  } else if ((char)payload[0] == '2') {
    activateSpeedLevel2();
  } else if ((char)payload[0] == '3') {
    activateSpeedLevel3();
  } else if ((char)payload[0] == '4') {
    activateSpeedLevel4();
  } else if ((char)payload[0] == 'L' && (char)payload[1] == 'O' && (char)payload[2] == 'N') {
    Serial.println("Light on");
  } else if ((char)payload[0] == 'L' && (char)payload[1] == 'O' && (char)payload[2] == 'F' && (char)payload[3] == 'F') {
    Serial.print("Light off");
  } else if ((char)payload[0] == 'H' && (char)payload[1] == 'O' && (char)payload[2] == 'N') {
    Serial.println("Heater on");
  } else if ((char)payload[0] == 'H' && (char)payload[1] == 'O' && (char)payload[2] == 'F' && (char)payload[3] == 'F') {
    Serial.print("Heater off");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (pubSubClient.connect("arduino-pool-control-client")) {
      Serial.println("connected");
      pubSubClient.publish("poolcontrol","Arduino online");
      pubSubClient.subscribe("poolcontrol");
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying (blink while waiting)
      //TODO
//      blinkLED(LED_LIGHT,10);
    }
  }
}

void blinkLED(int led, int times){
  //TODO
//  int initialState = digitalRead(led);
//  int stateToWrite = !initialState;
//  for(int i=0; i < times*2; i++){
//    digitalWrite(led, stateToWrite);
//    stateToWrite = !stateToWrite;
//    delay(250);
//  }
//  digitalWrite(led, initialState);
}

void updateShiftRegister()
{
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, MSBFIRST, leds);
   digitalWrite(latchPin, HIGH);
}

