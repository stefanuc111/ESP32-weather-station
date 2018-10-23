#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "esp_system.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "NetSerial.h"

//ESP WEMOS LOLIN32

const char* ssid = "M4nu3lNet";
const char* password = "Wlanman12345!";

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1800        /* Time ESP32 will go to sleep (in seconds) */
#define TIME_PROGRAMMING_MODE 30000 //30 seconds
#define LED_PIN 5
#define SERVO_PIN 18
#define SERVO_ENABLE_PIN 15

#define BATERRY_DIVIDER 1865.0
#define BATTERY_PIN 2

bool programmingMode = false;
hw_timer_t *programmingModeTimer = NULL;
Servo myservo;  // create servo object to control a servo
RTC_DATA_ATTR int lastServoPos = -1;

void IRAM_ATTR startDeepSleep() {
  Serial.println("Programming mode ended, going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
}

void startProgrammingTimer() {
  programmingModeTimer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(programmingModeTimer, &startDeepSleep, true);  //attach callback
  timerAlarmWrite(programmingModeTimer, TIME_PROGRAMMING_MODE * 1000, false); //set time in us
  timerAlarmEnable(programmingModeTimer);                          //enable interrupt
}

void enableServo() {
  pinMode(SERVO_ENABLE_PIN, OUTPUT);
  digitalWrite(SERVO_ENABLE_PIN, HIGH);
  myservo.attach(SERVO_PIN, 530, 2530);
}

bool servoEnabled = false;
bool writeServo(int val) {
  if (lastServoPos == val)
    return false;
  if (!servoEnabled) {
    enableServo();
    servoEnabled = true;
  }
  myservo.write(val);
  lastServoPos = val;
  return true;
}

void setup() {
  pinMode(LED_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN, LOW);



  Serial.begin(115200);
  Serial.println("Booting");

  long tot = 0;
  for (int i = 0; i < 10; i++) {
    tot += analogRead(2);
    delay(100);
  }
  float val = tot / 10.0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);


  if (wakeup_reason == 3) {
    programmingMode = false;
    getWeatherInfo();
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start();
  }

  Serial.print("Battery: ");
  float batVoltage = (val / BATERRY_DIVIDER) * 4.2;
  Serial.print(batVoltage);
  Serial.print("\t Val: ");
  Serial.println(val);

  int servoPos = 10 + (160 - (160 * ((batVoltage - 3) / (4.2 - 3))));

  if(servoPos < 10) servoPos = 10;
  if(servoPos > 170) servoPos = 170;
  
  writeServo(servoPos);

  delay(1000);

  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);

  programmingMode = true;
  Serial.println("Going into programming mode / manual boot");
  startProgrammingTimer();

  //  switch(wakeup_reason)
  //  {
  //    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
  //    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
  //    case 3  : Serial.println("Wakeup caused by timer"); break;
  //    case 4  : Serial.println("Wakeup caused by touchpad"); break;
  //    case 5  : Serial.println("Wakeup caused by ULP program"); break;
  //    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  //  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    timerWrite(programmingModeTimer, 0); //reset timer (feed watchdog)
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ota enabled!");

  getWeatherInfo();

}

void getWeatherInfo() {
  Serial.println("Updating whethar!!");

  HTTPClient http;

  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  //http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
  http.begin("https://query.yahooapis.com/v1/public/yql?u=c&q=select%20item.condition%20from%20weather.forecast%20where%20woeid%20in%20(select%20woeid%20from%20geo.places(1)%20where%20text%3D%22Giarre%22)%20and%20u=%27c%27%20&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys"); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonBuffer jsonBuffer;

      JsonObject& root = jsonBuffer.parse(payload);

      JsonObject& condition = root["query"]["results"]["channel"]["item"]["condition"];
      displayInfo(condition);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  digitalWrite(13, LOW);

}

void displayInfo(JsonObject & condition) {
  int code = condition["code"];
  float temp = condition["temp"];

  Serial.print("Current condition Code: ");
  Serial.print(code);
  Serial.print("\tTemperature: ");
  Serial.println(temp);

  //writeServo(180 - (temp / (float)40) * 180);

  if (writeServo(getCodePosition(code))) {
    delay(500);
  }
}

int getCodePosition(int code) {
  switch (code) {
    case 10:
    case 11:
    case 12:
    case 35:
    case 39:
    case 40:
      return 177; //rain
    case 27:
    case 28:
    case 29:
    case 30:
      return 138; //partially colud
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 47:
      return 99; //thuderstorm
    case 26:
      return 61; //cloudy
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 31:
    case 32:
    case 33:
    case 34:
      return 23; //sunny
    default:
      return 10;

  }
}

void loop() {
  ArduinoOTA.handle();
}
