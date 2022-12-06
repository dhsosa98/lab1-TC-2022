#include <DHT.h>

#include <Stepper.h>

#include <Wire.h>

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
float minTemp = 15.0;
float maxTemp = 30.0;
// Degree Angles Boundaries of the temperature adjustment system
float minAngle = 0;
float maxAngle = 180;
String input;
String selectedRibbon = "ribbon 2";
const byte I2C_SLAVE_ADDR = 0x01;

const char ASK_FOR_LENGTH = 'L';
const char ASK_FOR_DATA = 'D';

char request = ' ';
char requestIndex = 0;
String message; 

void setup() {
    Serial.begin(115200);
    myStepper.setSpeed(motSpeed);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onRequest(requestEvent);
    Wire.onReceive(receiveEvent);
    dht.begin();
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
    Wire.write(message.length());
    Serial.println(request);
    Serial.println(message.length());    
    char requestIndex = 0;
  }
  if(request == ASK_FOR_DATA)
  {
    if(requestIndex < (message.length() / 32)) 
    {
      Wire.write(message.c_str() + requestIndex * 32, 32);
      requestIndex ++;
      Serial.println(requestIndex); 
    }
    else
    {
      Wire.write(message.c_str() + requestIndex * 32, (message.length() % 32));
      requestIndex = 0;
    }
  }
}

void loop() {   
    //Reset the message to Master
    message="";
    //Read the Serial input
    input = Serial.readStringUntil('\n');
    // Change current control ribbon
    if (input.equals("start: ribbon 1")){
      selectedRibbon = "ribbon 1";
    }
    else if (input.equals("start: ribbon 2")){
      selectedRibbon = "ribbon 2";
    }
    Serial.println(selectedRibbon);
    // Handle Ribbons
    if (selectedRibbon.equals("ribbon 1")){
      Serial.println("Measuring... ");
      runStepperByTemperature();
    } else if (selectedRibbon.equals("ribbon 2")){
      Serial.println("Measuring... ");
      measureByHumidity();
    }     
    delay(500);
}

void runStepperByTemperature()
{   
    // Define the Json Structure
    StaticJsonDocument<136> doc; 
    StaticJsonDocument<52> actuator1;
    StaticJsonDocument<52> sensor1;
    JsonArray arrActuators = doc.createNestedArray("actuators");
    JsonArray arrSensors = doc.createNestedArray("sensors");

    // Read the temperature
    float t = dht.readTemperature();
    if (isnan(t)) {
        Serial.println("Failed to read Temp from DHT sensor!");
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

    // Fill the json according to the group interoperability standard
    actuator1["type"]="stepper";
    actuator1["current_value"]=currentInterval;
    sensor1["type"]="temp1";
    sensor1["current_value"]=t;
    arrActuators.add(actuator1);
    arrSensors.add(sensor1);
    doc["controller_name"]="Arduino-nano-1";

    // Convert Json to String
    serializeJson(doc, message);
    Serial.println(message.length()); 
}

void pass_right(){
  myStepper.step(stepsPerInterval); 
}

void pass_left(){
  myStepper.step(-stepsPerInterval); 
}


void measureByHumidity(){
  // Define the Json Structure
  StaticJsonDocument<199> doc;
  StaticJsonDocument<52> actuator1;
  StaticJsonDocument<52> actuator2;
  StaticJsonDocument<52> actuator3;
  StaticJsonDocument<52> sensor1;
  JsonArray arrActuators = doc.createNestedArray("actuators");
  JsonArray arrSensors = doc.createNestedArray("sensors");
  
  float h = dht.readHumidity();
  if (isnan(h)) {
     Serial.println("Failed to read Humidity from DHT sensor!");
     return;
  }
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println();

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
  // Fill the json according to the group interoperability standard
  actuator1["type"]="led1";
  actuator2["type"]="led2";
  actuator3["type"]="led3";
  sensor1["type"]= "hum1";
  sensor1["current_value"]= h;
  arrActuators.add(actuator1);
  arrActuators.add(actuator2);
  arrActuators.add(actuator3); 
  arrSensors.add(sensor1);
  doc["controller_name"]="Arduino-nano-1";
  
  // Convert Json to String
  serializeJson(doc, message);
  Serial.println(message.length()); 
}
