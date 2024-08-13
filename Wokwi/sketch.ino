// Libraries
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include "time.h"
#include <PubSubClient.h>
#include <ESP32Servo.h>

// Pins
#define MENU 34
#define UP 35
#define DOWN 32
#define SP 18
#define DHTPIN 5

// DHT sensor settings
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// LDRs
#define LDRL 36
#define LDRR 39

// Servo
#define Servopin 16
Servo servo1;
int thetaoffset = 30;
float D = 1;
int Gamma = 0.75;

// Switched on or off
bool power = true;

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi NTP settings
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET_DST 0
int UTC_OFFSET = 0;

// Variables for time and alarms
// Time variables
int curr_hours = 0;
int curr_minutes = 0;

// Alarm variables
bool alarm_stat[]   = {false, false, false};
bool alarm_stop[]   = {false, false, false};
int alarm_hour[]    = {0,0,0};
int alarm_minute[]  = {0,0,0};

// DHT variables
bool condition_stop = false;
int condition_stop_hour = 0;
float humidity;
float temperature;

char LDRmax[6];
char LDRside[2];

// Warning message
String warning_welcome = "  Welcome!";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void oledPrint(String text, int column, int row, int size, bool clear) {
  // -----
  // This function prints a string into the OLED screen
  // :: can set the text, column, row, size and whether you want to clear screen or not
  // -----

  // Clears if clear is set to 1
  if (clear) { display.clearDisplay(); }

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}



void printTime() {
  // -----
  // Prints the time in the required format
  // -----

  int minute = curr_minutes;
  updateTime();

  // Clears display every minute
  if (minute!=curr_minutes) { display.clearDisplay(); }

  oledPrint(tString(curr_minutes),70,10,4,0);         // Current time: Minutes
  oledPrint(tString(curr_hours),10,10,4,0);           // Current time: Hours
  oledPrint(":",50,10,4,0);                           

  // Displays alarms
  if (alarms() != "0") { oledPrint("*" + alarms(),10,50,1,0); }

  // Displays welcome/warning message
  oledPrint(warning_welcome, 58,50,1,0);
}


void updateTime() {
  // -----
  // Getting the time from the NTP server
  // -----

  struct tm timeinfo;

  // Check if there is any error in connection
  if (!getLocalTime(&timeinfo)) {
    oledPrint("Connection Error",0,0,2,1);
    delay(200);
    display.clearDisplay();
    return;
  }

  curr_hours   = timeinfo.tm_hour;
  curr_minutes = timeinfo.tm_min;
}


String tString(int time) {
  // -----
  // Converts integer to two numbered string
  // -----

  if (time >= 10) {
    return String(time);
  } else {
    return "0"+String(time);
  }
}


String alarms() {
  // -----
  // Checks for the number of alarms set
  // -----

  if (alarm_stat[0] && alarm_stat[1] && alarm_stat[2]) {
    return String(3);
  } else if ((alarm_stat[0] && alarm_stat[1])||(alarm_stat[1] && alarm_stat[2])||(alarm_stat[0] && alarm_stat[2])) {
    return String(2);
  } else if (!alarm_stat[0] && !alarm_stat[1] && !alarm_stat[2]) {
    return String(0);
  } else {
    return String(1);
  }
}





int checkButtonpress() {
  /// -----
  // Checks for the buttons pressed
  // :: Can only give one button that is pressed, as menu is set in such a way
  // ------
  
  if (!digitalRead(MENU)) {                           //normal - HIGH
    display.clearDisplay();
    return 1;
  } else if (!digitalRead(UP)) {                      //normal - HIGH
    display.clearDisplay();
    return 2;
  } else if (!digitalRead(DOWN)) {                    //normal - HIGH
    display.clearDisplay();
    return 3;
  } else {
    return 0;
  }
  delay(100);
}





void displayMenu(int item, bool alarm_disabled) {
  // -----
  // Displaying the format of the menu
  // -----

  // If the alarm is disabled within the session, option changes from 'Disbale All' to 'Disabled'
  String disable_message;
  if (alarm_disabled) {
    disable_message = "  Disabled";
  } else {
    disable_message = "  Disable All";
  }

  if (item==6) {
    // Menu scrolled down
    oledPrint("  Alarm 2    "+alarmMenuTime(2), 0,0,1,0);
    oledPrint("  Alarm 3    "+alarmMenuTime(3), 0,10,1,0);
    oledPrint("  Set Time", 0,20,1,0);
    oledPrint(disable_message, 0,30,1,0);
    oledPrint("> Exit ", 0,40,1,0);

  } else {
    // Menu scrolled up
    oledPrint("  Alarm 1    "+alarmMenuTime(1), 0,0,1,0);
    oledPrint("  Alarm 2    "+alarmMenuTime(2), 0,10,1,0);
    oledPrint("  Alarm 3    "+alarmMenuTime(3), 0,20,1,0);
    oledPrint("  Set Time", 0,30,1,0);
    oledPrint(disable_message, 0,40,1,0);
    
    // Cursor
    if (item == 1)      { oledPrint(">", 0,0,1,0);  } 
    else if (item == 2) { oledPrint(">", 0,10,1,0); } 
    else if (item == 3) { oledPrint(">", 0,20,1,0); }
    else if (item == 4) { oledPrint(">", 0,30,1,0); }
    else                { oledPrint(">", 0,40,1,0); }
  }
}





String alarmMenuTime(int alarm) {
  // -----
  // Formats time to be displayed next to alarms
  // -----

  if (alarm_stat[alarm-1]) {
    return tString(alarm_hour[alarm-1])+":"+tString(alarm_minute[alarm-1]);
  } else {
    return "  Set";
  }
}


void displayAlarmMenu(int alarm, int menu_item) {
  // -----
  // Displaying the format of the menu for setting the alarm
  // -----

  // If alarm is on, this shows OFF, if off, ON
  String off_on;
  if (!alarm_stat[alarm-1]) {
    off_on = "  Switch ON";
  } else {
    off_on = "  Switch OFF";
  }

  // Menu items                       
  oledPrint("  Hour       "+tString(alarm_hour[alarm-1]), 0,0,1,0);
  oledPrint("  Minute     "+tString(alarm_minute[alarm-1]), 0,10,1,0);
  oledPrint(off_on, 0,20,1,0);
  oledPrint("  Save", 0,30,1,0);

  /// Cursor
  if (menu_item == 1) { oledPrint(">", 0,0,1,0); } 
  else if (menu_item == 2) { oledPrint(">", 0,10,1,0); } 
  else if (menu_item == 3) { oledPrint(">", 0,20,1,0); } 
  else { oledPrint(">", 0,30,1,0); }
}


void setAlarm (int alarm) {
  // -----
  // Setting the alarm
  // -----

  // Enters an timed loop
  int menu_itr = 0;
  int menu_item = 1;                                  // Selected setting 
  int button = 0;

  while (menu_itr<6000) {                             // Menu auto-closes after 1 minute
      displayAlarmMenu(alarm, menu_item);
      delay(500);
      button = 0;
      button = checkButtonpress();
      menu_itr++;

      if (button == 2) {                              // Go up
        menu_item--;
        if (menu_item < 1) { menu_item = 4-menu_item; }

      } else if (button == 3) {                       // Go down
        menu_item++;
        if (menu_item > 4) { menu_item = menu_item-4; }

      } else if (button == 1) {     
        if (menu_item == 1) {                         // Increment hour
          alarm_hour[alarm-1]++;
          if (alarm_hour[alarm-1] > 23) {
            alarm_hour[alarm-1] = alarm_hour[alarm-1] - 24;
          }

        } else if (menu_item == 2) {                  // Increment minute
          alarm_minute[alarm-1]++;
          if (alarm_minute[alarm-1] > 59) {
            alarm_minute[alarm-1] = alarm_minute[alarm-1] - 60;

          }
        } else if (menu_item == 3) {                  // ON/OFF
          alarm_stat[alarm-1] = !alarm_stat[alarm-1];
        } else if (menu_item == 4) {                  // Save
          break;
        }
      }
  }
}


void checkAlarm(int alarm) {
  // -----
  // Checks is currently theere's an alarm supposed to be ringing
  // -----

  if ((alarm_stat[alarm - 1])&&(!alarm_stop[alarm-1])) {
    if ((alarm_hour[alarm - 1] == curr_hours) and (alarm_minute[alarm -1] == curr_minutes)) {
      while(true) {
        ring(1);                                      // Rings alarm
        if (checkButtonpress() == 1) {                // Checks if the alarm is snoozed by user
          alarmStop(alarm);
          break;
        }
      }

      delay(500);                                     // A delay so it doesn't go straight to Menu
    }
  }
}


void alarmStop(int alarm) {
  // -----
  // Stops the said alarm, by setting 'stop' to true
  // -----

  alarm_stop[alarm-1] = true;
}


void alarmReset() {
  // -----
  // Reactivates all alarms
  // -----

  alarm_stop[0] = false;
  alarm_stop[1] = false;
  alarm_stop[2] = false;
}


void disableAll() {
  // -----
  // Disables all alarms
  // -----

  alarm_stat[0]= false;
  alarm_stat[1]= false;
  alarm_stat[2]= false;
}





void displayTimezoneMenu(String timezone_name, int menu_item) {
  // -----
  // Displaying the menu for setting timezone
  // -----

  // Menu items
  oledPrint("  Timezone  "+timezone_name, 0,0,1,0);
  oledPrint("  Save", 0,10,1,0);

  // Cursor
  if (menu_item == 1) { oledPrint(">", 0,0,1,0); } 
  else if (menu_item == 2) { oledPrint(">", 0,10,1,0); }
}


void changeTimezone() {
  // -----
  // Changes the timezone from presaved values
  // -----

  // Contains all the standard timezones and their offsets
  int timezone_times[]    = {-43200,-39600,-36000, -34200, -32400, -28800, -25200, -21600, -18000, -14400, -12600, -10800, -7200, -3600, 
                             0, 3600, 7200, 10800, 12600, 14400, 16200, 18000, 19800, 20700, 21600, 23400, 25200, 28800, 31500, 32400, 
                             34200, 36000, 37800, 39600, 43200, 45900, 46800, 50400};
  String timezone_names[] = {" -12:00", " -11:00", " -10:00", "  -9:30", "  -9:00", "  -8:00", "  -7:00", "  -6:00", "  -5:00", "  -4:00", 
                             "  -3:30", "  -3:00", "  -2:00", "  -1:00", "+/-0:00", "  +1:00", "  +2:00", "  +3:00", "  +3:30", "  +4:00", 
                             "  +4:30", "  +5:00", "  +5:30", "  +5:45", "  +6:00", "  +6:30", "  +7:00", "  +8:00", "  +8:45", "  +9:00", 
                             "  +9:30", " +10:00", " +10:30", " +11:00", " +12:00", " +12:45", " +13:00", " +14:00"};
  
  int menu_itr = 0;
  int menu_item = 1;
  int button = 0;
  int timezone = 14;

  while (menu_itr < 6000) {
    displayTimezoneMenu(timezone_names[timezone], menu_item);
    delay(500);
    button = 0;
    button = checkButtonpress();
    menu_itr++;

    if (button == 2) {                                // Go UP
      menu_item--;
      if (menu_item<1) { menu_item = 2 - menu_item; }

    } else if (button == 3) {                         // Go DOWN
      menu_item++;
      if (menu_item>2) { menu_item = menu_item - 2; }

    } else if (button == 1) {
      if (menu_item == 1) {                           // Incrementing timezone
        timezone++;
        if (timezone>37) { timezone = timezone - 38; }

      } else {                                        // Saving and updating the NTP connection settings
        UTC_OFFSET = timezone_times[timezone];
        configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
        break;
      }
    }
  }
}





void checkTempHum() {
  // -----
  // Gives an alert if temperature or humidity is out of range
  // :: if the range is not in the range 26<=Temperature<=32 or 60<=Humidity<=80
  // -----
 
  humidity    = dht.readHumidity();
  temperature = dht.readTemperature();
  
  if ((temperature < 26)||(temperature > 32)||(humidity < 60)||(humidity > 80)) {
    while(true) {
      if (checkButtonpress() == 1) { 
        condition_stop = true;
        condition_stop_hour = curr_hours; 
      }
      if (condition_stop) { break; }
      ring(1);
      display.clearDisplay();
      warning_welcome = "  Warning!";
      printTime();
    }
    delay(1000);                                       // A delay so it doesn't go straight to Menu
    display.clearDisplay();
    printTime();

  } else {
    warning_welcome = "  Welcome!";
  }
}

void tempHumReset() {
  // -----
  // Resets alarm every hour
  // -----
  if (condition_stop) {
    if (curr_hours > condition_stop_hour) { condition_stop = false; }
  }
}





void ring(int number) {
  // -----
  // Playes a rythm
  // -----

  //notes
  int note_C = 523;
  int note_G = 784;

  if (number == 0) {                                  // Starting tone  
    tone(SP, note_C);
    delay(200);
    tone(SP, note_G);
    delay(600);
    noTone(SP);
  }

  if (number == 1) {                                  // Alarm 
    tone(SP, note_C);
    delay(200);
    noTone(SP);
    delay(200);
    tone(SP, note_C);
    delay(200);
    noTone(900); 
  }
}





void connectBroker() {
  while(!mqttClient.connected()) {
    oledPrint("Attempting MQTT",0,0,1,1);
    oledPrint("Connection...",0,0,1,1);
    if(mqttClient.connect("ESP32-Medibox")) {
      oledPrint("MQTT Connected",0,0,1,1);
      mqttClient.subscribe("medibox-onoff");
      mqttClient.subscribe("medibox-ctrl");
      mqttClient.subscribe("medibox-angle");
      delay(250);
      display.clearDisplay(); 
    } else {
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}



void updateLdr() {
  float Lval = analogRead(LDRL);
  float Rval = analogRead(LDRR);

  Lval = (4095-Lval)/4095;
  Rval = (4095-Rval)/4095;
  if (Lval>Rval) {
    Serial.println(Lval);
    String(Lval,2).toCharArray(LDRmax,6);
    LDRside[0] = 'L';
    D = 1.5;
  } else if (Rval>Lval) {
    Serial.println(Rval);
    String(Rval,2).toCharArray(LDRmax,6);
    LDRside[0] = 'R';
    D = 0.5;
  } else {
    Serial.println(Rval);
    String(Rval,2).toCharArray(LDRmax,6);
    LDRside[0] = 'M';
    D = 1;
  }
  mqttClient.publish("medibox-ldr",LDRmax);
  mqttClient.publish("medibox-side", LDRside);

  int maxval = max(Lval,Rval);
  int theta  = int(thetaoffset * D + (180-thetaoffset) * maxval * Gamma);
  int pos    = min(theta, 180); 
  servo1.write(pos);
}



void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharAr[length];
  for (int i=0; i<length; i++) {
    payloadCharAr[i] = (char)payload[i];
    //Serial.print(payloadCharAr[i]);
  }
  payloadCharAr[length] = '\0';
  

  if (strcmp(topic, "medibox-onoff")==0) {
    power = (payloadCharAr[0]=='1');
    display.clearDisplay();
  }

  if (strcmp(topic, "medibox-ctrl")==0) {
    Gamma = atof(payloadCharAr);
    Serial.print(Gamma);
  }

  if (strcmp(topic, "medibox-angle")==0) {
    thetaoffset = atof(payloadCharAr);
    Serial.print(thetaoffset);
  }
}

void setup() {
  // -----
  // Startup of MEDIBOX!
  // -----

  pinMode(MENU, INPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);

  Serial.begin(115200);
  Serial.println("Starting setup");

  // Initializing display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);
  oledPrint("Medi",10,0,4,1);
  oledPrint("Box!",10,30,4,0);
  ring(0);
  delay(2000);

  // Initializing Wifi
  WiFi.begin("Wokwi-GUEST", "", 6);
  display.clearDisplay();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    oledPrint("Establishing",0,0,1,0);
    oledPrint("Connection",0,10,1,0);
  }
  oledPrint("Connected!",0,0,1,1);
  oledPrint("Fetching time...",0,0,1,1);
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  // Setting up MQTT Server
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback);

  // Initializing DHT
  dht.begin();

  // Initializing servo
  servo1.attach(Servopin, 500, 2400);
}


void loop() {
  // -----
  // Main program
  // -----

  // MQTT connection
  if (!mqttClient.connected()) {
    connectBroker();
  }

  mqttClient.loop();

  if (power) {
    // Do the changes due to LDR
    updateLdr();

    printTime();

    // Check for alarms
    checkAlarm(1);
    checkAlarm(2);
    checkAlarm(3);

    checkTempHum();
    tempHumReset();

    // Resetting alarm everyday at 00:00h
    if ((curr_hours == 00)&&(curr_minutes == 00)) {
      alarmReset();
    }

    int button = 0;
    button = checkButtonpress();
    bool alarm_disabled = false;

    if (button == 1) {
      // Enters an timed loop, 
      int menu_itr = 0;
      int item = 1;                                     // Selected setting 

      while (menu_itr<1200) {                           // Menu auto-closes after roughly 2 minutes of inactivity
        displayMenu(item, alarm_disabled);
        delay(100);

        button = 0;
        button = checkButtonpress();
        menu_itr++;

        if (button == 2) {                              // Goes up
          item--;
          if (item < 1) { item = 6-item; }

        } else if (button == 3) {                       // Goes down
          item++;
          if (item > 6) { item = item-6; }

        } else if (button == 1) {                       
          if (item == 1) { setAlarm(1); }               // Change alarm 1
          else if (item == 2) { setAlarm(2); }          // Change alarm 2 
          else if (item == 3) { setAlarm(3); }          // Change alarm 3
          else if (item == 4) { changeTimezone(); }     // Change timezone
          else if (item == 5) {                         // Disable all alarms
            alarm_disabled = true;
            disableAll();
          } else { break; }
        }

      }
    } 
  }
  delay(100);
}

