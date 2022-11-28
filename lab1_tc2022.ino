#include <DHT.h>

#include <Stepper.h>

#include <Wire.h>

#include <DateTime.h>

#include <ArduinoJson.h>

#define SENSOR_TEMP_PIN 2

#define DHTTYPE DHT11

DHT dht(SENSOR_TEMP_PIN, DHTTYPE);

int stepsPerRevolution = 1024; // Steps to make a revolution
Stepper myStepper(stepsPerRevolution, 8,10,9,11); // 8,10,9,11 are the pins where the stepper is allocated

int currentInterval=0; 
int intervals = 16; // Amount of linear intervals that the temperature adjustment system have
int stepsPerInterval = stepsPerRevolution/intervals;
// Stepper Motor speed
int motSpeed = 20; 
// Celcius Degree Temperature Boundaries that the system need to handle
float minTemp = 15;
float maxTemp = 30;
// Degree Angles Boundaries of the temperature adjustment system
float minAngle = 0;
float maxAngle = 180;
String input;
String selectedRibbon = "ribbon 1";

String json;
DynamicJsonDocument doc(1024);

const byte I2C_SLAVE_ADDR = 0x20;

void setup() {
    Serial.begin(9600);
    myStepper.setSpeed(motSpeed);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onRequest(requestEvent);
    Wire.onReceive(receiveEvent);
    dht.begin();
}

const char ASK_FOR_LENGTH = 'L';
const char ASK_FOR_DATA = 'D';

char request = ' ';
char requestIndex = 0;

void SerializeObject(JsonArray sensors, JsonArray actuators)
{
    doc["slave"] = "Arduino-nano-1";
    doc["date"] = DateTime.now();
    doc["actuators"] = actuators;
    doc["sensors"] = sensors;
    serializeJson(doc, json);
}

void receiveEvent(int bytes)
{
   while (Wire.available())
   {
     request = (char)Wire.read();
   }
}
void requestEvent()
{
   if(request == ASK_FOR_LENGTH)
   {
      Wire.write(json.length());
      char requestIndex = 0;
   }
   if(request == ASK_FOR_DATA)
   {
      if(requestIndex < (json.length() / 32)) 
      {
         Wire.write(json.c_str() + requestIndex * 32, 32);
         requestIndex ++;
      }
      else
      {
         Wire.write(json.c_str() + requestIndex * 32, (json.length() % 32));
         requestIndex = 0;
      }
   }
}

void loop() {   
    input = Serial.readStringUntil('\n');
    if (input == String("start: ribbon 1")){
      selectedRibbon = "ribbon 1";
    }
    else if (input == String("start: ribbon 2")){
      selectedRibbon = "ribbon 2";
    }
    
    if (selectedRibbon == String("ribbon 1")){
      runStepperByTemperature();
    } else if (selectedRibbon == String("ribbon 2")){
      measureByHumidity();
    }
    delay(500);
}

void runStepperByTemperature()
{
  float t = dht.readTemperature();
    if (isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }
    float tempAngleRatio = maxAngle/(maxTemp - minTemp); //Ratio between Degree Angle by Temperature
    float stepsByAngleRatio = stepsPerRevolution/maxAngle; //Ratio between Stepper Steps by Degree Angle
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C ");
    // Correct the current temperature if across the boundaries
    if (t<=minTemp){
      t=minTemp;
    }
    else if(t>=maxTemp){
      t=maxTemp;
    }    
    float tempAngle = t*(tempAngleRatio) - minTemp*(tempAngleRatio); //Compute the current temperature by Angle Degree
    float numberStepsByAngle = tempAngle*(stepsByAngleRatio); //Compute the amount of steps that need the stepper with the current Angle temperature
    Serial.print("Movement: ");
    Serial.print(tempAngle);
    Serial.println(" °Angle Degrees ");
    Serial.println("Current Steps: ");
    Serial.println(numberStepsByAngle);
    // Go clockwise until the stepper reach the correct interval
    while (trunc(numberStepsByAngle / stepsPerInterval)>currentInterval){
        pass_left();
        currentInterval++;
    }
    // Go counterclockwise until the stepper reach the correct interval
    while (trunc(numberStepsByAngle / stepsPerInterval)<currentInterval){
        pass_right();
        currentInterval--;
    }
    Serial.println("Interval: ");
    Serial.println(currentInterval);
    Serial.println("Interval Angle Degrees: ");
    Serial.println(currentInterval*(maxAngle/intervals));
    StaticJsonDocument<200> actuator1;
    StaticJsonDocument<200> sensor1;
    DynamicJsonDocument docActuators(1024);
    JsonArray arrActuators = docActuators.to<JsonArray>();
    DynamicJsonDocument docSensors(1024);
    JsonArray arrSensors = docSensors.to<JsonArray>();
    actuator1["type"]="stepper";
    actuator1["current_interval"]=currentInterval;
    sensor1["type"]="temperature";
    sensor1["current_value"]=t;
    arrActuators[0]=actuator1;
    arrSensors[0]=sensor1;
    SerializeObject(arrSensors, arrActuators);
}

void pass_right(){
  myStepper.step(stepsPerInterval); 
}

void pass_left(){
  myStepper.step(-stepsPerInterval); 
}


void measureByHumidity(){
  float h = dht.readHumidity();
  Serial.print("Humidity: ");
  Serial.print(h);
  if (isnan(h)) {
     Serial.println("Failed to read from DHT sensor!");
     return;
  }
  StaticJsonDocument<200> actuator1;
  StaticJsonDocument<200> actuator2;
  StaticJsonDocument<200> actuator3;
  StaticJsonDocument<200> sensor1;
  DynamicJsonDocument docActuators(1024);
  JsonArray arrActuators = docActuators.to<JsonArray>();
  DynamicJsonDocument docSensors(1024);
  JsonArray arrSensors = docSensors.to<JsonArray>();
  actuator1["type"]="led";
  actuator2["type"]="led";
  actuator3["type"]="led";
  sensor1["type"]= "humidity";
  sensor1["current_value"]= h;
  if (h<=25){
    // turn off all leds
      digitalWrite(3, LOW);
      digitalWrite(4, LOW);
      digitalWrite(5, LOW);
      actuator1["current_value"]=0;
      actuator2["current_value"]=0;
      actuator3["current_value"]=0;
    }
    else if ((h>25) and (h<=50)){
      // turn on one led
      digitalWrite(3, HIGH);
      digitalWrite(4, LOW);
      digitalWrite(5, LOW);
      actuator1["current_value"]=1;
      actuator2["current_value"]=0;
      actuator3["current_value"]=0;
    }
    else if ((h>50) and (h<=75)) {
      // turn on two leds
      digitalWrite(3, HIGH);
      digitalWrite(4, HIGH);
      digitalWrite(5, LOW);
      actuator1["current_value"]=1;
      actuator2["current_value"]=1;
      actuator3["current_value"]=0;
    }
    else {
      // turn on all leds
      digitalWrite(3, HIGH);
      digitalWrite(4, HIGH);
      digitalWrite(5, HIGH);
      actuator1["current_value"]=1;
      actuator2["current_value"]=1;
      actuator3["current_value"]=1;
    }
    arrActuators[0] = actuator1;
    arrActuators[1] = actuator2;
    arrActuators[2] = actuator3; 
    arrSensors[0] = sensor1;
    SerializeObject(arrSensors, arrActuators);
}
