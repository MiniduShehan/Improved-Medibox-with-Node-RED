#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define L_LDR 34
#define R_LDR 35
#define ServoPin 26

#define DHT_PIN 15
#define BUZZER 19



float minimumAngle = 30;  //setting default values for min angle and confactor
float controllingFactor = 0.75;
float D;
float angle;     // to save servo position


float L_LDRval;
float R_LDRval;    //to save anlog read from ldr
float highLDRval;
char highLDRside;  //to save corrrected brightness level from ldr



char preset = 'n';

WiFiClient espClient;
PubSubClient mqttClient(espClient);  //allows the ESP32 to publish messages to topics and subscribe to topics on an MQTT broker
DHTesp dhtSensor;
Servo myservo;// create servo object


WiFiUDP ntpUDP; // to initiate ntp client
NTPClient timeClient(ntpUDP);

char tempAr[6]; // charcters 6 use vana nissa 100.25 vage
char highLDRvalStr[10];  //to save publishing light intensitivity


bool isScheduledON = false;
unsigned long scheduledOnTime;



void setup()
{
  pinMode(L_LDR, INPUT);
  pinMode(R_LDR, INPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(BUZZER, LOW);
  myservo.attach(ServoPin, 500, 2400); //min angle on servo and max angle(500,2400)

  Serial.begin(115200);

  setupWifi();
  setupMqtt();

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); // initialize dht sensor

  timeClient.begin();
  timeClient.setTimeOffset(5.5 * 3600); // seting time to sri lankas time zone
}

void loop()
{
  if (!mqttClient.connected())
  { // to check if we are connected to the mqtt client
    connectToBroker();
  }

  mqttClient.loop(); // to check there are incoming msgs and to send the published values when looping
  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("ENTC-ADMIN-TEMP", tempAr);
  checkSchedule();
  getHighestLightIntensity();
  setServo();
  delay(1000);
}

//function1

void getHighestLightIntensity()
{
  L_LDRval = analogRead(L_LDR); // inverse lux min =>anlog read high
  R_LDRval = analogRead(R_LDR);

  if (L_LDRval <= R_LDRval)
  {
    highLDRval = 1 - (L_LDRval / 4063);
    highLDRside = 'L';
    Serial.print(highLDRval);
    Serial.print(highLDRside);
    // Convert float to string
    dtostrf(highLDRval, 5, 2, highLDRvalStr); // Format: 5 characters, 2 decimal places
    mqttClient.publish("ENTC-ADMIN-LightIntensity", highLDRvalStr);
  }
  else
  {
    highLDRval = 1 - (R_LDRval / 4063);
    highLDRside = 'R';
    Serial.print(highLDRval);
    Serial.print(highLDRside);
    // Convert float to string
    dtostrf(highLDRval, 5, 2, highLDRvalStr); // Format: 5 characters, 2 decimal places
    mqttClient.publish("ENTC-ADMIN-LightIntensity", highLDRvalStr);
  }
}

//function2

void setServo()
{
  if (highLDRside == 'L')
  {
    D = 1.5;
  }
  else
  {
    D = 0.5;
  }
  
  angle = (minimumAngle * D) + ((180 - minimumAngle) * highLDRval * controllingFactor);

  if (angle > 180)
  {
    myservo.write(180);
  }
  else if (angle < 0)
  {
    myservo.write(0);
  }
  else
  {
    myservo.write(angle);
  }
}

//function3
unsigned long getTime()
{
  timeClient.update();
  return timeClient.getEpochTime();
}

//function4

void checkSchedule()
{
  if (isScheduledON)
  {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime)
    {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP", "1"); // main switch on
      mqttClient.publish("ENTC-ADMIN-SCH-ESP-ON", "0"); // next shedule switch off so buzzer can be controlled from main switch
      Serial.println("Scheduled ON");
    }
  }
}

//function5

void buzzerOn(bool on)
{
  if (on)
  {

    tone(BUZZER, 256); // on in 256hz
  }
  else
  {
    noTone(BUZZER);
  }
}

//function6

void connectToBroker()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-12345645454"))
    {
      Serial.println("connected");
      mqttClient.subscribe("ENTC-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ENTC-ADMIN-SCH-ON");
      mqttClient.subscribe("ENTC-ADMIN-minimumAngle");
      mqttClient.subscribe("ENTC-ADMIN-controllingFactor");
      mqttClient.subscribe("ENTC-ADMIN-DropDown");
    }
    else
    {
      Serial.print("failed to subscribe");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

//function7

void setupMqtt()
{
  mqttClient.setServer("test.mosquitto.org", 1883); // setting up server
  mqttClient.setCallback(receiveCallback);          // subscribe will happen in call back function
}

//function8

void updateTemperature()
{
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6); // string with two decimal points then itll  convert to charcter array
}

//function9

void receiveCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");
  // Serial.println();

  char payloadCharAr[length]; // to save coming data
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]); // adding data to payloadchararc
    payloadCharAr[i] = (char)payload[i];
  }

  if (strcmp(topic, "ENTC-ADMIN-MAIN-ON-OFF") == 0)
  { // if topic =ENTC-ADMIN-MAIN-ON-OFF strcmp will equal to 0
    buzzerOn(!(payloadCharAr[0] == '0')); // payloadCharAr[0] =1 will turn on the buzzer
  }
  else if (strcmp(topic, "ENTC-ADMIN-SCH-ON") == 0)
  {

    if (payloadCharAr[0] == 'N')
    {
      isScheduledON = false;
    }
    else
    {
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr); // array to long
    }
  }
  else if ((strcmp(topic, "ENTC-ADMIN-minimumAngle") == 0))
  {
    minimumAngle = atol(payloadCharAr);
    mqttClient.publish("ENTC-ADMIN-SET-DROPDOWN-DEFAULT", "n");
  }

  else if ((strcmp(topic, "ENTC-ADMIN-controllingFactor") == 0))
  {
    controllingFactor = atol(payloadCharAr);
    mqttClient.publish("ENTC-ADMIN-SET-DROPDOWN-DEFAULT", "n");
  }
  else if (strcmp(topic, "ENTC-ADMIN-DropDown") == 0)
  {
    if (payloadCharAr[0] == 'a')
    {
      preset = 'a';
      minimumAngle = 10;
      controllingFactor = 0.20;
      char minimumAngleStr[10];      // Define char array to hold the converted minimumAngle
      char controllingFactorStr[10]; // Define char array to hold the converted controllingFactor

      // Convert float values to strings
      dtostrf(minimumAngle, 6, 2, minimumAngleStr); // Format: 5 characters, 2 decimal places
      dtostrf(controllingFactor, 6, 2, controllingFactorStr);
      mqttClient.publish("ENTC-ADMIN-PRE-MIN-ANGLE", minimumAngleStr);
      mqttClient.publish("ENTC-ADMIN-PRE-CON-FACTOR", controllingFactorStr);
    }
    else if (payloadCharAr[0] == 'b')
    {
      preset = 'b';
      minimumAngle = 20;
      controllingFactor = 0.40;
      char minimumAngleStr[10];      // Define char array to hold the converted minimumAngle
      char controllingFactorStr[10]; // Define char array to hold the converted controllingFactor

      // Convert float values to strings
      dtostrf(minimumAngle, 6, 2, minimumAngleStr); // Format: 5 characters, 2 decimal places
      dtostrf(controllingFactor, 6, 2, controllingFactorStr);
      mqttClient.publish("ENTC-ADMIN-PRE-MIN-ANGLE", minimumAngleStr);
      mqttClient.publish("ENTC-ADMIN-PRE-CON-FACTOR", controllingFactorStr);
    }
    else if (payloadCharAr[0] == 'c')
    {
      preset = 'c';
      minimumAngle = 40;
      controllingFactor = 0.60;
      char minimumAngleStr[10];      // Define char array to hold the converted minimumAngle
      char controllingFactorStr[10]; // Define char array to hold the converted controllingFactor

      // Convert float values to strings
      dtostrf(minimumAngle, 6, 2, minimumAngleStr); // Format: 5 characters, 2 decimal places
      dtostrf(controllingFactor, 6, 2, controllingFactorStr);
      mqttClient.publish("ENTC-ADMIN-PRE-MIN-ANGLE", minimumAngleStr);
      mqttClient.publish("ENTC-ADMIN-PRE-CON-FACTOR", controllingFactorStr);
    }
    else if (payloadCharAr[0] == 'n')
    {
      preset = 'n';
    }
  }
}

//function10

void setupWifi()
{

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", ""); // connecting to wokwie wifi

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
