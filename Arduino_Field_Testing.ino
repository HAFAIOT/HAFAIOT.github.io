#include <DHT.h>
#include <DHT_U.h>
#include <MKRGSM.h>
#include <RTCZero.h>
#include <SPIMemory.h>
#include "ThingSpeak.h"
#include <ArduinoJson.h>

const char PINNUMBER[] = " ";
const char GPRS_APN[] = "hologram";
const char GPRS_LOGIN[] = " ";
const char GPRS_PASSWORD[] = " ";

//DHT temperature information
#define DHTPIN 2
#define DHTTYPE DHT22
char jsonBuffer[6000] ="[";

//ThingSpeak IDs
unsigned long ThingSpeakChannelIDTemp = ;   //Update
unsigned long ThingSpeakChannelIDHigh = ;   //Update
const char * ThingSpeakWriteKeyTemp = "";   //Update
const char * ThingSpeakReadKeyHigh = "";    //Update
char TSserver[] = "api.thingspeak.com";

//Flash information
int tempStart=1;
int forceDayCycle;
const int FlashChipSelect = 5;
int dailyLocation=7900000;

//IFTTT data
char IFTTT_Event[] = "highTemp";
char IFserver[] = "maker.ifttt.com"; 
char IFTTT_Key[] = "";                    //Update

//Input and Output
int buttonState = 0;
const int ledPin = 6; //MKR GSM 1400 on board led on 6
const int buttonPin = 7;

//Objects and Functions
GPRS gprs;
RTCZero rtc;
GSM gsmAccess;
GSMClient client;
DHT dht(DHTPIN,DHTTYPE);
SPIFlash flash(FlashChipSelect);

String print2digits(int number);
void httpRequest(char* jsonBuffer);
void updatesJson(char* jsonBuffer);
char *append_str(char *here, char *s);
int sendNotification(String dateTime);
char *append_ul(char *here, unsigned long u);

bool matched = false;

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  while(!Serial);
  
  Serial.println(F("Arduino_Field_Testing"));
  
  connectInternet();

  dht.begin(); //Startup dht22
  rtc.begin(); //Startup real time clock
  rtc.setEpoch(gsmAccess.getTime()); //Set rtc to the current time gotten from the internet
  flash.begin(MB(2));
  ThingSpeak.begin(client);

  forceDayCycle=flash.readLong(dailyLocation);
  
  Serial.println(F("GSM initialized."));
  gsmAccess.shutdown();
  delay(2000);
  Serial.println(F("\nStarting Sleep Cycle.\n"));
  byte minu=(((rtc.getMinutes()+10)-rtc.getMinutes()%10)%60);
  
  rtc.setAlarmMinutes(minu);
  rtc.setAlarmSeconds(0);
  rtc.enableAlarm(rtc.MATCH_MMSS);
    
  rtc.attachInterrupt(Alarma);
  rtc.standbyMode();
  digitalWrite(ledPin, LOW);
}

void loop() {
  if(matched){
    //Every 10 minutes take temperature, check if it is too high
    if(rtc.getEpoch()%86400<=86300&&rtc.getEpoch()%86400>=100&&forceDayCycle<=86500){
      //Serial.println(F("Getting temperature"));
      digitalWrite(ledPin, HIGH);
  
      //Store current time and temperature
      int i;
      for(i=0;i<300;i+=2){
        if(flash.readLong(tempStart+(32*i))<=1){
          flash.writeLong(tempStart+(32*i),rtc.getEpoch());
          dht.readTemperature(true);
          delay(3000);
          int temp = dht.readTemperature(true);
          //Serial.print(F("Current temp: "));
          //Serial.println(temp);
          flash.writeLong(tempStart+(32*(i+1)),temp);
          break;
        }
      }
      delay(3000);
  
      //Check for high temp
      float overheatingTemp=flash.readFloat(dailyLocation+32);
      if(dht.readTemperature(true)>=overheatingTemp){
        delay(3000);
        if(dht.readTemperature(true)>=overheatingTemp){
          //Serial.println(F("OverheatingTemp is "));
          //Serial.println(overheatingTemp);
          String notification = "Your tunnel is currently overheating with a temperature of: ";
          notification+= String(dht.readTemperature(true));
          notification+= "F";
          connectInternet();
          sendNotification(notification);
          gsmAccess.shutdown();
        }
      }
      delay(3000);
  
      //Advance Day Cycle
      forceDayCycle+=600;
      flash.eraseBlock32K(dailyLocation);
      flash.writeLong(dailyLocation,forceDayCycle);
      flash.writeFloat(dailyLocation+32,overheatingTemp);
      //Serial.println(F("Finished getting temperature"));
      digitalWrite(ledPin, LOW);
    }
  
    //Send in the temperature once a day or if has not run for a day
    if(rtc.getEpoch()%86400>=86300||rtc.getEpoch()%86400<=100||forceDayCycle>=86500){
      //Serial.println("Preforming daily upload and download");
      digitalWrite(ledPin, HIGH);
      delay(200);
      digitalWrite(ledPin, LOW);
      delay(200);
      digitalWrite(ledPin, HIGH);
      
      connectInternet();
      updatesJson(jsonBuffer);
      httpRequest(jsonBuffer);
  
      delay(20000);
      flash.eraseBlock32K(dailyLocation);
      flash.writeLong(dailyLocation,0);
      flash.writeFloat(dailyLocation+32,ThingSpeak.readFloatField(ThingSpeakChannelIDHigh,1,ThingSpeakReadKeyHigh));
      forceDayCycle=0;
      gsmAccess.shutdown();
      delay(5000);
      //Serial.println("Finished daily upload and download");
      digitalWrite(ledPin, LOW);
    }

    //Reset the alarm
    matched = false;
    
    byte minu=(((rtc.getMinutes()+10)-rtc.getMinutes()%10)%60);
  
    rtc.setAlarmMinutes(minu);
    rtc.setAlarmSeconds(0);
    rtc.enableAlarm(rtc.MATCH_MMSS);
    
    rtc.attachInterrupt(Alarma);
    rtc.standbyMode();
  }
}

void Alarma(){
  matched = true;
}

//Connect to the internet
void connectInternet(){
  boolean connected = false;
  while(!connected){
    if((gsmAccess.begin() == GSM_READY)&&(gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY)){
      Serial.println(F("Connected"));
      connected = true;
    }
    else{
      Serial.println(F("Not connected"));
      int i;
      for(i=0;i<5;i++){
        digitalWrite(ledPin, HIGH); //LED will flash if the SIM can't connect
        delay(250);
        digitalWrite(ledPin, LOW);
        delay(250);
      }
    }
  }
}

// Updates the josnBuffer with data
void updatesJson(char* jsonBuffer){
  // JSON format for updates paramter in the API
  int i;
  for(i=0;i<300;i+=2){
    if(flash.readLong(tempStart+(32*i))>=1){
      strcat(jsonBuffer,"{\"created_at\":");
      unsigned long deltaT = flash.readLong(tempStart+(32*i));
      size_t lengthT = String(deltaT).length();
      char temp[11];
      String(deltaT).toCharArray(temp,lengthT+1);
      strcat(jsonBuffer,temp);
      strcat(jsonBuffer,",");
      int fahr = flash.readLong(tempStart+(32*(i+1)));
      strcat(jsonBuffer, "\"field1\":");
      lengthT = String(fahr).length();
      String(fahr).toCharArray(temp,lengthT+1);
      strcat(jsonBuffer,temp);
      strcat(jsonBuffer,"},");
    }
    else{
      break;
    }
  }
  size_t len = strlen(jsonBuffer);
  jsonBuffer[len-1] = ']';
  //Serial.println(jsonBuffer);
}

// Updates the ThingSpeakchannel with data
void httpRequest(char* jsonBuffer) {
  // JSON format for data buffer in the API
  char data[6000] = "{\"write_api_key\":\"";
  strcat(data,ThingSpeakWriteKeyTemp);
  strcat(data,"\",\"updates\":");
  strcat(data,jsonBuffer);
  strcat(data,"}");
  // Close any connection before sending a new request
  client.stop();
  String data_length = String(strlen(data)+1); //Compute the data buffer length
  //Serial.println(data);
  // POST data to ThingSpeak
  if (client.connect(TSserver, 80)) {
    client.print("POST /channels/");
    client.print(ThingSpeakChannelIDTemp);
    client.println("/bulk_update.json HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("User-Agent: mw.doc.bulk-update (MKR_GSM_1400)");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.println("Content-Length: "+data_length);
    client.println();
    client.println(data);
  }
  else {
    //Serial.println("Failure: Failed to connect to ThingSpeak");
  }
  delay(250); //Wait to receive the response
  client.parseFloat();
  String resp = String(client.parseInt());
  //Serial.println("Response code:"+resp); // Print the response code. 202 indicates that the TSserver has accepted the response
  jsonBuffer[0] = '['; //Reinitialize the jsonBuffer for next batch of data
  jsonBuffer[1] = '\0';
  flash.eraseBlock32K(tempStart);
}

//Send a notification through IFTTT that will show the input string
int sendNotification(String notification) {

    // connect to the Maker event server
    if(client.connect(IFserver, 80)){
      //Serial.println("Connected to IFTTT server");
    }
    else{
      //Serial.println("Failed to connected to IFTTT server");
      return 0;
    }

    // construct the POST request
    char post_rqst[256+notification.length()];    // hand-calculated to be big enough

    char *p = post_rqst;
    p = append_str(p, "POST /trigger/");
    p = append_str(p, IFTTT_Event);
    p = append_str(p, "/with/key/");
    p = append_str(p, IFTTT_Key);
    p = append_str(p, " HTTP/1.1\r\n");
    p = append_str(p, "Host: maker.ifttt.com\r\n");
    p = append_str(p, "Content-Type: application/json\r\n");
    p = append_str(p, "Content-Length: ");

    // we need to remember where the content length will go, which is:
    char *content_length_here = p;

    // it's always two digits, so reserve space for them (the NN)
    p = append_str(p, "NN\r\n");

    // end of headers
    p = append_str(p, "\r\n");

    // construct the JSON; remember where we started so we will know len
    char *json_start = p;

    // As described - this example reports a pin, uptime, and "via#tuinbot"
    p = append_str(p, "{\"value1\":\"");
    
    size_t lengthT = notification.length();
    char temp[lengthT*2];
    notification.toCharArray(temp,lengthT+1);

    p = append_str(p, temp);
    p = append_str(p, "\"}");

    // go back and fill in the JSON length
    // we just know this is at most 3 digits
    int i = strlen(json_start);
    if(i>=100){
      content_length_here[0] = '0' + ((i/100)%10);
      content_length_here[1] = '0' + ((i/10)%10);
      content_length_here[2] = '0' + (i%10);
    }
    else{
      content_length_here[0] = '0' + (i/10);
      content_length_here[1] = '0' + (i%10);
    }

    //Send the POST to the server
    //Serial.println(post_rqst);
    client.print(post_rqst);
    client.stop();
    return 1;
}

//Similiar to strcat but for char
char *append_str(char *here, char *s) {
    while (*here++ = *s++)
  ;
    return here-1;
}

//Similiar to strcat but for numbers
char *append_ul(char *here, unsigned long u) {
    char buf[20];       // we "just know" this is big enough

    return append_str(here, ultoa(u, buf, 10));
}

//Changes any one digit number to a string with a "0" in frount
//any number with two or more digits will be converted to a string with no changes
String print2digits(int number) {
  String result;
  if (number < 10) {
    result="0"; // print a 0 before if the number is < than 10
    result+=String(number);
  }
  else{
    result=String(number);
  }
  return result;
}
