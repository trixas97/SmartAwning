#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <MyRealTimeClock.h>
#include <Servo.h>
#include <DHT.h>
#include <EEPROM.h>
#define sensorPin A0
#define DHTTYPE DHT11
const int photoPin = 12;
const int waterPin = 14;
const int dhtPin = 13;



const char* ssid     = "WIFI NAME";
const char* password = "WIFI PASSWORD";
const String device_name = "NodeMCU_Tenda";
const int port = 80;
const int publicPort = 300;
const String publicIPApi = "http://api.ipify.org/?format=text";
String publicIp = "-";


const long utcOffsetInSeconds = 7200;
String response = "";

ESP8266WebServer server(port);
HTTPClient http;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
Servo servo;
MyRealTimeClock myRTC(4,0,2);
DHT dht(dhtPin, DHTTYPE);


boolean tendaState = false;
boolean programEnable = true;
boolean waterEnable = true;
boolean lightEnable = true;
boolean programOpen = false;
boolean waterOpen = false;
boolean lightOpen = false;

int dh;
int dm;
int nh;
int nm;
int tendaValue = 0;
int servoMax = 180;
int sensorValue;
int temperature = 0;
int humidity = 0;
int waterLimit = 100;
int lightLimit = 100;

int countClock;
int countLight;
int countWater;
int countTempHum;

void setup() {
  EEPROM.begin(1);
  Serial.begin(115200);
  dht.begin();

  initWifi();
  initRTC();
  initServer();
  myRTC.updateTime();

  dh = EEPROM.read(0);
  dm = EEPROM.read(1);
  nh = EEPROM.read(2);
  nm = EEPROM.read(3);

  servo.attach(5);
  servo.write(0);

  
  pinMode(waterPin, OUTPUT);
  pinMode(photoPin, OUTPUT);

  countClock = 0;
  countLight = 0;
  countWater = 0;

  
  
  delay(2000);
  temperature = dht.readTemperature();
  delay(500);
  humidity = dht.readHumidity();
  while(temperature > 50){
    temperature = dht.readTemperature();
    delay(500);
    humidity = dht.readHumidity();
  }
  Serial.println("Temperature: " + String(temperature) + "C");
  Serial.println("Humidity: " + String(humidity) + "%");
}

void loop() { 
  if(countClock == 20){
    myRTC.updateTime();
    controlProgramTime();
//    statePrint(); 
    countClock = 0;
  }
  
  if(countWater == 20){
    if((countLight != 22) && (waterEnable == true)){
      waterSensor();
    }
    countWater = 0;
  }

  if(countLight == 22){
    lightSensor();
    countLight = 0;
  }

  if(countTempHum == 3600){
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    while((temperature > 50) || (temperature < -50)){
      temperature = dht.readTemperature();
      humidity = dht.readHumidity();
    }
    countTempHum = 0;
  }
  
  server.handleClient();

  countTempHum++;
  countClock++;
  countLight++;
  countWater++;
  delay(500); 
}



//Init WiFi Connection
void initWifi(){
  WiFi.begin(ssid, password);
  while( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ");
  Serial.println("Connected to: " + String(ssid));
  Serial.println("IP address is: " + WiFi.localIP().toString());
  http.begin(publicIPApi.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
    publicIp = http.getString();
    Serial.println("Public Ip: " + publicIp + ":" + String(publicPort));
  }
  http.end();
}

//Init Server to listen requests
void initServer(){
  api();
  server.begin();
  Serial.println("Web Server Started!");
}


//Init RTC from NTP Server
void initRTC(){
  timeClient.begin();
  timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  String currentDate = String(monthDay) + "-" + String(currentMonth) + "-" + String(currentYear);
  
  myRTC.setDS1302Time(timeClient.getSeconds(), timeClient.getMinutes(), timeClient.getHours(), timeClient.getDay(), monthDay, currentMonth, currentYear);
}


void servoMotor(int degrees){
  if (degrees >= 0) {
    servo.write(degrees);
    if(degrees > 0){
      tendaState = true;
    }
    else { 
      tendaState = false;
    }
    tendaValue = degrees;
    // Serial.println("Servo moved (" + String(tendaValue) + ")");
  }
}


void controlProgramTime(){
  if((programEnable == true) && (dh == myRTC.hours) && (dm == myRTC.minutes)){
    if((tendaState == false)){
      servoMotor(servoMax);
    }
    programOpen = true;
  }
  else if ((programEnable == true) && (nh == myRTC.hours) && (nm == myRTC.minutes)){   
    if(tendaState == true && (programOpen == true)){
      programOpen = false;
      tendaValue = 0;
      waterSensor();
      lightSensor();
      if((waterOpen == false) && (lightOpen == false)){
        servoMotor(0);
      }
    }
    programOpen = false;
  }
}

void waterSensor(){
  if((waterEnable == true) && (programOpen == false)){
    digitalWrite(waterPin, HIGH);
    sensorValue = analogRead(sensorPin);
    // Serial.println("Water value: " + String(sensorValue));
    if ((sensorValue > waterLimit) && (tendaValue < servoMax)) {
      servoMotor(servoMax);
      waterOpen = true;
    }
    else if((sensorValue <= waterLimit) && (tendaState == true) && (lightOpen == false) && (tendaValue == servoMax)){
      servoMotor(0);
      waterOpen = false;
    }
    digitalWrite(waterPin, LOW);
  }
}

void lightSensor(){
  if((lightEnable == true) && (programOpen == false)){
    digitalWrite(photoPin, HIGH);
    sensorValue = analogRead(sensorPin);
    // Serial.println("Photo value: " + String(sensorValue));
    if ((sensorValue > lightLimit) && (tendaValue < servoMax)) {
      servoMotor(servoMax);
      lightOpen = true;
    }
    else if((sensorValue <= lightLimit) && (tendaState == true) && (waterOpen == false) && (tendaValue == servoMax)){
      servoMotor(0);
      lightOpen = false;
    }
    digitalWrite(photoPin, LOW);
  }
}


void api(){
  server.on("/detect", []() {
    if(server.args() > 0){
      if((server.argName(0) == "smart") && (server.arg(0) == "awning")){
        response =  "{ ";
        response += "\"name\": \"" + device_name + "\", ";
        response += "\"ip\": \"" + String(WiFi.localIP().toString()) + "\", ";
        response += "\"public_ip\": \"" + publicIp + "\", ";
        response += "\"public_port\": \"" + String(publicPort) + "\", ";
        response += "\"mac\": \"" + String(WiFi.macAddress()) + "\"";
        response += " }";
      }
    }        
    server.send(200, "application/json", response);
    response = "";
  });

  server.on("/device/info", [](){
    char timevar[5] = "";
    
    response =  "{ ";
    response += "\"temperature\": " + String(temperature) + ", ";
    response += "\"humidity\": " + String(humidity) + ", ";
    response += "\"program_enable\": " + String(programEnable) + ", ";
    response += "\"water_enable\": " + String(waterEnable) + ", ";
    response += "\"light_enable\": " + String(lightEnable) + ", ";
    response += "\"awning_state\": " + String(tendaState) + ", ";
    sprintf(timevar, "%02d:%02d",dh,dm);
    response += "\"program_open\": \"" + String(timevar) + "\", ";
    sprintf(timevar, "%02d:%02d",nh,nm);
    response += "\"program_close\": \"" + String(timevar) + "\", ";
    response += "\"awning_value\": " + String((100 * tendaValue) / servoMax);
    response += " }";
    server.send(200, "application/json", response);
    response = "";
  });

  server.on("/device/set/bar", [](){
    if(server.args() > 0){
      if((server.argName(0) == "awning_value_percent") && (server.arg(0).toInt() >= 0) && (server.arg(0).toInt() <= 100)){  
        
        if((waterEnable == true) && (lightEnable == true)){
          waterEnable = false;
          lightEnable = false;
          servoMotor((servoMax * server.arg(0).toInt()) / 100);
          waterEnable = true;
          lightEnable = true;
        }else if(waterEnable == true){
          waterEnable = false;
          servoMotor((servoMax * server.arg(0).toInt()) / 100);
          waterEnable = true;
        }else if(lightEnable == true){
          lightEnable = false;
          servoMotor((servoMax * server.arg(0).toInt()) / 100);
          lightEnable = true;
        }else { servoMotor((servoMax * server.arg(0).toInt()) / 100); };
        
        response = "{ ";
        response += "\"response\": 200, ";
        response += "\"text\": \"Servo moved to " + String(tendaValue) + " degree (Awning" + server.arg(0) + "%)\"";
        response += " }";
      }
    }
    server.send(200, "application/json", response);
    response = "";
  });

  server.on("/device/set/program", [](){
    if(server.args() > 0){
      String text;
      if(server.argName(0) == "enable"){
        if(server.arg(0) == "true"){
          programEnable = true;
          text = "Program enabled";
        }else if(server.arg(0) == "false"){
          programEnable = false;
          text = "Program disabled";
        }
      }else if((server.argName(0) == "open_hour") && (server.argName(1) == "open_min")){
        EEPROM.write(0,server.arg(0).toInt());
        EEPROM.commit();
        EEPROM.write(1,server.arg(1).toInt());
        EEPROM.commit();
        text = "Changed open time";
      }else if((server.argName(0) == "close_hour") && (server.argName(1) == "close_min")){
        EEPROM.write(2,server.arg(0).toInt());
        EEPROM.commit();
        EEPROM.write(3,server.arg(1).toInt());
        EEPROM.commit();
        text = "Changed close time";
      }else{
        text = "No action";
      }

      dh = EEPROM.read(0);
      dm = EEPROM.read(1);
      nh = EEPROM.read(2);
      nm = EEPROM.read(3);

      response = "{ ";
      response += "\"response\": 200, ";
      response += "\"text\": \"" + text + " (" + String(dh) + ":" + String(dm) + " - " + String(nh) + ":" + String(nm) + ")\"";
      response += " }";
    }
    server.send(200, "application/json", response);
    response = "";
  });

  server.on("/device/set/water", [](){
    if(server.args() > 0){
      if(server.argName(0) == "sensor"){
        String text;
        if(server.arg(0) == "true"){
          waterEnable = true;
          text = "Water Sensor enabled";
        }
        else if(server.arg(0) == "false"){
          waterEnable = false;
          waterOpen = false;
          text = "Water Sensor disabled";
        }
        response = "{ ";
        response += "\"response\": 200, ";
        response += "\"text\": \"" + text + "\"";
        response += " }";
      }
    }
    server.send(200, "application/json", response);
    response = "";
  });

  server.on("/device/set/light", [](){
    if(server.args() > 0){
      if(server.argName(0) == "sensor"){
        String text;
        if(server.arg(0) == "true"){
          lightEnable = true;
          text = "Light Sensor enabled";
        }
        else if(server.arg(0) == "false"){
          lightEnable = false;
          lightOpen = false;
          text = "Light Sensor disabled";
        }
        response = "{ ";
        response += "\"response\": 200, ";
        response += "\"text\": \"" + text + "\"";
        response += " }";
      }
    }
    server.send(200, "application/json", response);
    response = "";
  });
  
}


// void statePrint(){
//   Serial.println("Real Time: " + String(myRTC.hours) + ":" + String(myRTC.minutes) + ":" + String(myRTC.seconds));
//   Serial.println("Program Time (Open): " + String(dh) + ":" + String(dm));
//   Serial.println("Program Time (Close): " + String(nh) + ":" + String(nm));
//   Serial.println("Program Control (now): " + String(programOpen));
//   Serial.println("Temperature: " + String(temperature) + "C");
//   Serial.println("Humidity: " + String(humidity) + "%");
//   Serial.println("Tenda State: " + String(tendaState));
//   Serial.println("Tenda Value: " + String(tendaValue));
//   Serial.println("Water Enable: " + String(waterEnable));
//   Serial.println("Water Open: " + String(waterOpen));
//   Serial.println("Light Enable: " + String(lightEnable));
//   Serial.println("Light Open: " + String(lightOpen));
//   Serial.println("-------------------------------------------------------------"); 
// }
//