// Import required libraries
#include <WiFi.h>
#include <DHT11.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define DHTPIN 27     // Digital pin connected to the DHT sensor
DHT11 dht(DHTPIN);

//global variables
String requested_position_change;

//Data pins for 74HC595
int clockPin = 13; //Pin connected to SH_CP (11) of 74HC595
int latchPin = 12; //Pin connected to ST_CP (12) of 74HC595
int dataPin = 14; //Pin connected to DS (14) of 74HC595

//Data pins for 74HC595
int clockPin2 = 13; //Pin connected to SH_CP (11) of 74HC595
int latchPin2 = 12; //Pin connected to ST_CP (12) of 74HC595
int dataPin2 = 14; //Pin connected to DS (14) of 74HC595

const char* ssid = "????"; //WIFI SSID
const char* password = "???"; //WIFI password

const char* mqtt_server = "IPADDRESS"; //MQTT Broker IP address
const char* topic = "esp32";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {

  //set pins to output so you can control the shift register
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  pinMode(latchPin2, OUTPUT);
  pinMode(clockPin2, OUTPUT);
  pinMode(dataPin2, OUTPUT);

  Serial.begin(115200);

  //After running all ouputs should be off
  allOutputsOff();
  
  //Setup wifi
  setup_wifi();

  //MQTT Client
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {

  /* ========== Check for connection to MQTT server and reconnect if not connected ========== */
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  /* ========== Read DHT Sensor and publish to MQTT server every 10 seconds ========== */
  String humidity;
  String temperature;

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    
    temperature = readDHTTemperature();
    char tempString[8]; // Convert the value to a char array
    temperature.toCharArray(tempString,8);
    Serial.print("Temperature: ");
    Serial.println(temperature);
    client.publish("esp32/temperature", tempString);

    delay(2000);

    humidity = readDHTHumidity();
    char humString[8];// Convert the value to a char array
    humidity.toCharArray(humString,8);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    client.publish("esp32/humidity", humString);
  }
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void valvecontrol(int direction, int requested_position_change, int valve_number) {

  //Valve position control
  //
  //Valve can open 0-100%, function requires requested_position_change as input
  //Assume it requires 100 turns to fully open the valve, 
  //To make one turn, 50 cycles are required
  //Opening 25% is then 25 turns * 50 cycles
  //Opening 50% is the 50 turns * 50 cycles 
  //
  //The valve must be able to move in two directions
  //This can be done by reversing the sequence of the outputs
  //0 is forward, 1 is backwards
  //This also explicitly means that requested_position_change is always a positive number (because of combination with direction)
  //
  //Valve cannot be open more that 100% or less than 0%
  //The state of the valve position must be stored permanently and available throughout the programme
  //If the requested position_move or the sum of the current position + requested_move is larger or smaller than the limits the programme should continue with the request upto the limits
  //
  //This function furthermore requires information about which valve has to move

  int cycles = 6; //how many cycles to make the motor shaft make one turn

  Serial.print("valve change: ");
  Serial.println(requested_position_change);
  Serial.print("valve number: ");
  Serial.println(valve_number);
  Serial.print("direction: ");
  Serial.println(direction);

  //For running a stepper motor 4 outputs must be used where 1 output is high at a time
  int i = 0; 
  int j = 0;
  int k = 0;
  //int clockPin;
  //int latchPin;
  //int dataPin;

  //Output array
  int output[3][4] = { 0 };  //initialise at 0;

  int pattern[4] = { B00000101, B00001001, B00001010, B00000110 };
  
  //forwards[4] =  { B0101, B1001, B1010, B0110 }
  //backwards[4] = { B0110, B1010, B1001, B0101 };

  switch (valve_number) {
    case 0:
      for (i=0; i<4; i++) {
        output[0][i] = pattern[i];
        output[1][i] = 0;
        output[2][i] = 0;
      }
      break;  
    case 1:
      for (i=0; i<4; i++) {
        output[0][i] = pattern[i];
        //shift four positions
        output[0][i] = output[0][i] << 4;
        output[1][i] = 0;
        output[2][i] = 0;
      }
      break;
    case 2:
      for (i=0; i<4; i++) {
        output[0][i] = 0;
        output[1][i] = pattern[i];
        output[2][i] = 0;
      }
      break;
    case 3:
      for (i=0; i<4; i++) {
        output[0][i] = 0;
        output[1][i] = pattern[i];
        //shift four positions
        output[1][i] = output[1][i] << 4;
        output[2][i] = 0;
      }
      break;
    case 4:
      for (i=0; i<4; i++) {
        output[0][i] = 0;
        output[1][i] = 0;
        output[2][i] = pattern[i];
      }
      break;
    case 5:
      for (i=0; i<4; i++) {
        output[0][i] = 0;
        output[1][i] = 0;
        output[2][i] = pattern[i];
        //shift four positions
        output[2][i] = output[2][i] << 4;
      }
      break;
  }

  if (direction == 0) {
    //Direction is 0 (forwards)
    //Loop to run the number of cycles to make one turn * the number of turns to make requested_position_change
    for (j=0; j < (cycles * requested_position_change); j++) {
      //Loop to make one cycle of the four coils in the motor
      for (k = 0; k < 4; k++) {
        //ground latchPin and hold low for as long as you are transmitting
        //Serial.println(k);
        digitalWrite(latchPin, 0);
        shiftOut(dataPin, clockPin, MSBFIRST, output[2][k]);
        shiftOut(dataPin, clockPin, MSBFIRST, output[1][k]);
        shiftOut(dataPin, clockPin, MSBFIRST, output[0][k]);
        //take the latch pin high so the LEDs will light up:
        digitalWrite(latchPin, HIGH);
        delay(10);
      }
    }
    //after running all outputs should be off
    allOutputsOff();
  }

  else {
    //Direction is 1 (backwards)
    //Loop to run the number of cycles to make one turn * the number of turns to make requested_position_change
    for (j=0; j < (cycles*requested_position_change); j++) {
      //Loop to make one cycle of the four coils in the motor
      for (k = 3; k > -1; k--) {
        //ground latchPin and hold low for as long as you are transmitting
        //Serial.println(k);
        digitalWrite(latchPin, 0);
        shiftOut(dataPin, clockPin, MSBFIRST, output[2][k]);
        shiftOut(dataPin, clockPin, MSBFIRST, output[1][k]);
        shiftOut(dataPin, clockPin, MSBFIRST, output[0][k]);
        //take the latch pin high so the LEDs will light up:
        digitalWrite(latchPin, HIGH);
        delay(10);
      }
    }
    //After running all ouputs should be off
    allOutputsOff();
  }
}

void allOutputsOff(void) {
  digitalWrite(latchPin, 0);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  digitalWrite(latchPin, HIGH);
}

String readDHTTemperature() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)
  int t = dht.readTemperature();
  //Serial.println(t);
  if (isnan(t)) {    
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    //Serial.println(t);
    return String(t);
  }
}

String readDHTHumidity() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  int h = dht.readHumidity();
  //Serial.println(h);
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    //Serial.println(h);
    return String(h);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/cmd");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {

  //The data from MQTT is in JSON format:
  /*
    {
      "Sensor": "valve"
      "valve_number": 5
      "direction": 0
      "requested_position_change": 25
    }
  */

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
  return;
  }

  deserializeJson(doc, message, length);
  int valve_number = doc["valve_number"];
  int direction = doc["direction"];
  int requested_position_change = doc["requested_position_change"];


  //String requested_position_change;
  
  //for (int i = 0; i < length; i++) {
  //  Serial.print((char)message[i]);
  //  requested_position_change += (char)message[i];
  //}
  //Serial.println();

  // If a message is received on the topic esp32/cmd then call valve change function 
  if (String(topic) == "esp32/cmd") {
    Serial.print("New valve position: ");

    /* ========== Call valve movement function when website sets toggle_gpio to true ========== */
    /* This is needed because async webserver cannot have delay functions included */
    //valvecontrol(0, requested_position_change.toInt(), 3);
    valvecontrol(direction, requested_position_change, valve_number);
  }
}