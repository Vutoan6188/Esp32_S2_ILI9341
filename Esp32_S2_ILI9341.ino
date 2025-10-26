#define AA_FONT_10 "fonts/NotoSans-Bold10"
#define AA_FONT_SMALL "fonts/NotoSansBold15"// 15 point sans serif bold
#define AA_FONT_LARGE "fonts/NotoSansBold36" // 36 point sans serif bold
#define AA_FONT_XLARGE "fonts/NotoSans-Bold60"
#define FIRMWARE_VERSION "0.0.1"
/***************************************************************************************
**                          Load the libraries and settings
***************************************************************************************/
#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// Additional functions
#include "GfxUi.h"          // Attached to this sketch
#include <TJpg_Decoder.h>
#include <FS.h>
#include <LittleFS.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <EEPROM.h>
#include <BlynkSimpleEsp32.h>
// check All_Settings.h for adapting to your needs
#include "All_Settings.h"

#include <JSON_Decoder.h> // https://github.com/Bodmer/JSON_Decoder

#include "NTP_Time.h"     // Attached to this sketch, see that tab for library needs

/***************************************************************************************
**                          Define the globals and class instances
***************************************************************************************/

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);  // Khởi tạo sprite
//TFT_eSprite json = TFT_eSprite(&tft);
boolean booted = true;
bool GetData = false;
bool GetMoonPhase = false;
GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw functions TODO: pull outside of a class

long lastDownloadUpdate = millis();
/***************************************************************************************
**                          Declare prototypes
***************************************************************************************/
void updateData();
void updateDataMoon();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawTemHum();
void drawGraphThrough();
//void drawJson();
String jsonglobal;
const char* getMeteoconIcon(String weatherIcon, int Cloudcover, int isday);
void drawAstronomy();
void drawSeparator(uint16_t y);

void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour);
void drawCurrentWeather();

int leftOffset(String text, String sub);
int rightOffset(String text, String sub);
int splitIndex(String text);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

struct CurrentWeather {
  float temperature;
  float pressure;
  float windSpeedKmph;
  int humidity;
  int windDirectionDegrees;
  int UVindex;
  int Cloudcover;
  String weatherIcon;
  String weatherStat;
  String timeupdate;
  float rainmm;
  int isday;
};
CurrentWeather *currentWeather;

struct DailyWeather {
  float moonage;
  String sunrise;
  String sunset;
  String moonrise;
  String moonset;
};
DailyWeather* dailyWeather;

int pwmChannel = 0; // Selects channel 0
int frequence = 1000; // PWM frequency of 1 KHz
int resolution = 8; // 8-bit resolution, 256 possible values
int pwmPin = 39; //2 for Esp32DevKit

#include <DHT.h>
#define DHTPIN 5 //22 for Esp32DevKit
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
long last_GRAPH = millis();
unsigned long currentMillis = 0;
unsigned long millisSyncTime = 0;
unsigned long connectMillis = 0;
unsigned long prev4s = 0;
float tempdht;
int humdht;
//  Graph Through
float graphHumIN;
float graphTemIN;
int x[240]; //graphics buffer HI
int y[240]; //secondary graphics buffer HI
int z[240]; // TI
int a[240]; // TI
// WiFi Received Signal Strength Indicator
int valueWiFiStrong;
int WiFiRSSI;
//  Graph
const int UPDATE_GRAPH_SECS = 12 * 60UL;    //6 * 60UL;  // 60UL = 1minute => 6 minute // 12 * 60UL  => 12p
/***************************************************************************************
**                          Variables for WiFiManager
***************************************************************************************/
char locationKey[40] = "";
char apiKey[48] = "";
//char authKey[40] = "";
char shortKey[11];  // 10 ký tự + null terminator

// 🧰 Thông tin GitHub
const char* GITHUB_USER = "Vutoan6188";
const char* GITHUB_REPO = "Esp32-BlackPCB-Weather-Station";
const char* FIRMWARE_FILENAME = "Esp32_S2_ILI9341.ino.bin";  // Tên file .bin trong phần release

WiFiClientSecure client;

#define BUTTON_PIN 0
/***************************************************************************************
**                          Setup
***************************************************************************************/
void setup() {
  //  Serial.begin(115200);
  // Configuration of channel 0 with the chosen frequency and resolution
  ledcSetup(pwmChannel, frequence, resolution);
  // Assigns the PWM channel to pin 21
  ledcAttachPin(pwmPin, pwmChannel);
  // Create the selected output voltage
  //DIM
  ledcWrite(pwmChannel, 255);
  tft.setRotation(0); // (0) for Esp32DevKit
  //  tft.setRotation(4);  // for ST7789
  tft.begin();
  spr.createSprite(240, 48);
  tft.fillScreen(TFT_BLACK);
  LittleFS.begin();
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true); // May need to swap the jpg colour bytes (endianess)

  // Draw splash screen
  if (LittleFS.exists("/splash/weather_icon.jpg") == true) {TJpgDec.drawFsJpg(0, 40, "/splash/weather_icon.jpg", LittleFS);
  }  
  delay(3000);
//    delay(360000);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Kích hoạt pull-up nội bộ cho GPIO09

  EEPROM.begin(512); // Khởi tạo EEPROM
  WiFiManager wifiManager;
  // Kiểm tra trạng thái nút nhấn khi khởi động
  if (digitalRead(BUTTON_PIN) == LOW) { // Nếu nút nhấn được nhấn
    //Serial.println("Nút nhấn được nhấn, xóa cấu hình cũ...");
    tft.loadFont(AA_FONT_10, LittleFS);
    tft.setTextDatum(BC_DATUM); // Bottom Centre datum
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text
    tft.drawString(" ", 120, 260);
    tft.drawString("Clearing the old configuration", 120, 260);
    // Xóa cấu hình WiFi và thông số
    wifiManager.resetSettings(); // Xóa cấu hình WiFi đã lưu
    EEPROM.write(0, 0); // Xóa locationKey, apiKey và authKey trong EEPROM
    EEPROM.commit();

    //Serial.println("Cấu hình đã được xóa. Hãy cài đặt lại.");
  } else {
    // Nếu không nhấn nút, đọc locationKey và apiKey từ EEPROM
    EEPROM.get(0, locationKey);
    EEPROM.get(40, apiKey);
//    EEPROM.get(88, authKey);
  }
  WiFiManagerParameter custom_locationKey("locationKey", "Enter Location_Key", locationKey, 40);
  WiFiManagerParameter custom_apiKey("apiKey", "Enter API_Key", apiKey, 48);
//  WiFiManagerParameter custom_authKey("authKey", "BLYNK_AUTH_TOKEN", authKey, 40);
  
  wifiManager.addParameter(&custom_locationKey);
  wifiManager.addParameter(&custom_apiKey);
//  wifiManager.addParameter(&custom_authKey);

  // Tự động kết nối WiFi
  if (!wifiManager.autoConnect("Weather_Station_Config")) {
    //Serial.println("Không thể kết nối WiFi");
    ESP.restart(); // Khởi động lại nếu kết nối không thành công
  }
  tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text
  tft.drawString(" ", 120, 260);

  // Clear bottom section of screen
  tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);

  tft.loadFont(AA_FONT_10, LittleFS);
  tft.setTextDatum(BL_DATUM); // Bottom Centre datum
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.drawString("Connecting to WiFi", 70, 240);
  tft.drawString("SSID: " + WiFi.SSID(), 0, 255);
  tft.drawString("IP: " + WiFi.localIP().toString(), 0, 270);
  delay(2000);
  // Lưu thông số mới vào EEPROM
  strcpy(locationKey, custom_locationKey.getValue());
  strcpy(apiKey, custom_apiKey.getValue());
//  strcpy(authKey, custom_authKey.getValue());
  EEPROM.put(0, locationKey);
  EEPROM.put(40, apiKey);
//  EEPROM.put(88, authKey);  
  EEPROM.commit();

  tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
  tft.setTextDatum(BL_DATUM); // Bottom Centre datum
  tft.setTextColor(TFT_GREEN, TFT_BLACK);  
  tft.drawString("Location: " + String(locationKey).substring(0, 25), 0, 240);
  tft.drawString("Api Key: " + String(apiKey).substring(0, 25), 0, 255);
//  tft.drawString("Blynk Auth Token: " + String(authKey).substring(0, 20), 0, 285);
  delay(3000);

  dht.begin();

  tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
  tft.loadFont(AA_FONT_10, LittleFS);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Original by: blog.squix.org", 0, 240);
  tft.drawString("Adapted by: Bodmer", 0, 255);
  tft.drawString("Firmware version: " + String(FIRMWARE_VERSION), 0, 270);
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  checkFirmwareUpdate();
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);
  tft.drawString("Fetching weather data...", 120, 240);
  tft.unloadFont();
  delay(2000);
//  Blynk.config(authKey);
  // Fetch the time
  udp.begin(localPort);
  syncTime();

  // Graph Range Initialization
  for (int i = 240; i >= 0; i--) {
    x[i] = 9999;
    y[i] = 9999;
    z[i] = 9999;
    a[i] = 9999;
  }
}

/***************************************************************************************
**                          Loop
***************************************************************************************/
void loop() {
//  Blynk.run();
  currentMillis = millis();
  if (currentMillis - prev4s > 3000)  {
    prev4s = currentMillis;
    drawTemHum();
  }
  if ((currentMillis - connectMillis >= 600000) && (WiFi.status() != WL_CONNECTED)) { // Check every 10 minutes
    connectMillis = currentMillis;
    WiFi.disconnect();
    WiFi.reconnect();
  }
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);
  int h = hour(local_time);
  int m = minute(local_time);
  int s = second(local_time);
// Check if we should update weather information
  if (booted || ((m == 0 || m == 30) && !GetData)) {
    updateData(h, m);
  }
  if (m != 0 && m != 30) {
    GetData = false;
  }
  if (booted || ((h == 4) && (m == 59)  && (s > 50)&& !GetMoonPhase)) {
    updateDataMoon(h, m);
  }
  if (h != 5) {
    GetMoonPhase = false;
  }
  // If minute has changed then request new time from NTP server
  if (booted || minute() != lastMinute)
  {
    // Update displayed time first as we may have to wait for a response
    drawTime();
    lastMinute = minute();
  }
  if (booted || currentMillis - millisSyncTime >= 60000) {
    millisSyncTime = currentMillis;
    // Request and synchronise the local clock
    syncTime();
  }
  booted = false;
}

// === OTA CHECK ===
void checkFirmwareUpdate() {
  // Clear bottom section of screen
  tft.fillRect(240, 206, 400, 320 - 206, TFT_BLACK);
  tft.loadFont(AA_FONT_10, LittleFS);
  tft.setTextDatum(BL_DATUM); // Bottom Centre datum
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  Serial.println("\n🔍 Check firmware on GitHub...");
  tft.drawString("Check firmware on GitHub...", 0, 240);

  HTTPClient http;
  client.setInsecure();  // Bỏ xác minh chứng chỉ SSL
  http.begin(client, String("https://api.github.com/repos/") + GITHUB_USER + "/" + GITHUB_REPO + "/releases/latest");
  http.addHeader("User-Agent", "ESP32-OTA-Agent");

  int httpCode = http.GET();
  if (httpCode == 200) {
    String json = http.getString();
    int tagStart = json.indexOf("\"tag_name\":\"") + 12;
    int tagEnd = json.indexOf("\"", tagStart);
    String latestVersion = json.substring(tagStart, tagEnd);

    Serial.printf("🔹 Firmware latest: %s\n", latestVersion.c_str());
    Serial.printf("🔹 Firmware current: %s\n", FIRMWARE_VERSION);
    tft.drawString("Firmware latest: " + String(latestVersion.c_str()), 0, 255);
    tft.drawString("Firmware current: " + String(FIRMWARE_VERSION), 0, 270);
    delay(3000);
    if (latestVersion != FIRMWARE_VERSION) {
      tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
      Serial.println("⚡ New firmware detected — starting OTA update...");
      tft.drawString("New firmware detected — starting OTA update...", 0, 240);  
      String binUrl = "https://github.com/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) +
                      "/releases/download/" + latestVersion + "/" + FIRMWARE_FILENAME;

      Serial.println("📦 File URL: " + binUrl);
      tft.drawString("File URL: " + String(binUrl), 0, 255);
      HTTPClient https;
      https.begin(client, binUrl);
      https.addHeader("User-Agent", "ESP32-OTA-Agent");
      https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // 🟢 Theo dõi redirect 302

      int code = https.GET();
      Serial.printf("🔁 HTTP Code: %d\n", code);
      tft.drawString("HTTP code: " + String(code), 0, 300);
      if (code == 200) {
        int len = https.getSize();
        WiFiClient *stream = https.getStreamPtr();

        if (Update.begin(len)) {
          size_t written = Update.writeStream(*stream);
          if (written == len) {
            tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
            Serial.println("✅ Firmware write complete!");
            tft.drawString("Firmware write complete!", 0, 240);
            delay(3000);
          } else {
            Serial.printf("⚠️ Firmware write incomplete — missing data! (%d/%d bytes)\n", written, len);
            tft.drawString("Firmware write incomplete — missing data!" + String(written, len), 0, 240);
          }

          if (Update.end()) {
            if (Update.isFinished()) {
              Serial.println("🎉 Update complete — rebooting...");
              tft.drawString("Update complete — rebooting...", 0, 255);
              delay(3000);
              ESP.restart();
            } else {
              Serial.println("❌ Update chưa hoàn tất!");
              tft.drawString("Update incomplete...", 0, 255);
            }
          } else {
            Serial.printf("❌ Update error: %s\n", Update.errorString());
            tft.drawString("Update error...", 0, 255);
          }
        } else {
          Serial.println("❌ Không thể bắt đầu update!");
          tft.drawString("Can't update...", 0, 255);
        }
      } else {
        Serial.printf("❌ Tải file thất bại! HTTP code: %d\n", code);
        tft.drawString("Fail load file..." + String(code), 0, 270);
      }
      https.end();
    } else {
      tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
      Serial.println("✅ The latest version");
      tft.drawString("The latest version", 0, 240);
      delay(3000);
    }
  } else {
    tft.fillRect(0, 200, 240, 320 - 200, TFT_BLACK);
    Serial.printf("❌ Không thể kiểm tra phiên bản! HTTP code: %d\n", httpCode);
    tft.drawString("Can't check firmware" + String(httpCode), 0, 285);
  }

  http.end();
}

void drawTemHum() {
  humdht = dht.readHumidity();
  tempdht = dht.readTemperature();
  if (isnan(humdht) || isnan(tempdht)) {
    //    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  tft.loadFont(AA_FONT_LARGE, LittleFS);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("88.8°"));
  tft.drawString(String(tempdht, 1) + "°", 240, 61);
  //  tft.fillRect(143, 57, 4, 26, TFT_CYAN);
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  //  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.drawString(String(humdht) + "%", 240, 97);
  //  tft.fillRect(194, 92, 4, 12, TFT_ORANGE);
//  Blynk.virtualWrite(V1,tempdht);
//  Blynk.virtualWrite(V2,humdht);  
  WiFiRSSI = WiFi.RSSI();
  valueWiFiStrong = map(WiFi.RSSI(), -35, -120, 240, 0);  //thử bỏ mạch phát wifi cách 20cm thì chỉ lên được -35dbm
  if (WiFi.status() != WL_CONNECTED) {
    valueWiFiStrong = 0;
    WiFiRSSI = -120;
  }

  tft.drawLine(0, 48, 240, 48, TFT_RED);  //0x4228
  tft.drawLine(0, 48, valueWiFiStrong, 48, TFT_CYAN);
  drawGraphThrough();
}

void drawGraphThrough() {
  if (millis() - last_GRAPH > 1000UL * UPDATE_GRAPH_SECS )
  {
    last_GRAPH = millis();
    tft.fillRect(0, 260, 480, 50, TFT_BLACK);
    //      tft.fillRect(240, 260, 480, 40, 0x4228);
    //  Graph Line
    for (int m = 0; m < 60; m = m + 10) {
      tft.drawLine(0, 310 - m, 480, 310 - m, 0x4228);   //0x4228
    }
    for (int l = 0; l < 240; l = l + 10) {
      tft.drawLine(0 + l, 260, 0 + l, 310, 0x4228);
    }
    tft.loadFont(AA_FONT_10, LittleFS);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextPadding(tft.textWidth("8"));

    //    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    //  tft.drawString("0", 0, 317);
    tft.drawString("10", 0, 307);
    tft.drawString("20", 0, 297);
    tft.drawString("30", 0, 287);
    tft.drawString("40", 0, 277);
    //tft.drawString("50", 0, 267);
    //      tft.drawString("60", 0, 255);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("20%", 20, 307);
    tft.drawString("40%", 20, 297);
    tft.drawString("60%", 20, 287);
    tft.drawString("80%", 20, 277);
    //tft.drawString("100%", 20, 267);
    graphHumIN = map(humdht, 0, 100, 310, 260);
    graphTemIN = map(tempdht, 0, 50, 310, 260);
    x[240] = graphHumIN;
    z[240] = graphTemIN;
    for (int k = 240; k >= 0; k--) {
      tft.drawLine(k, z[k], k - 1, z[k - 1], TFT_CYAN); // Wind Direction
      tft.drawLine(k, x[k], k - 1, x[k - 1], TFT_ORANGE);   // Wind Gust
      a[k - 1] = z[k];
      y[k - 1] = x[k];
    }
    for (int i = 240; i >= 0; i--) {
      x[i] = y[i]; //send the shifted data back to the variable x
      z[i] = a[i];
    }
    tft.fillRect(0, 311, 240, 9, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("36", 55, 322);
    tft.drawString("24", 115, 322);
    tft.drawString("12", 175, 322);
    //    tft.drawString("0", 234, 322);
    tft.fillRect(0, 314, 5, 3, TFT_CYAN);
    tft.drawString("T", 6, 322);
    tft.fillRect(22, 314, 5, 3, TFT_ORANGE);
    tft.drawString("H", 28, 322);    
  }
}
/***************************************************************************************
**                          Draw the clock digits
***************************************************************************************/
void drawTime() {
  spr.fillSprite(TFT_BLACK);
  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);
  spr.loadFont(AA_FONT_SMALL, LittleFS);
  spr.setTextPadding(tft.textWidth("WEDNESDAY"));
  spr.drawString(dayStr(weekday(local_time)), 44, 20);
  //  spr.pushSprite(0, 0);
  String dateNow = "";
  dateNow +=  day(local_time);
  dateNow += "/";
  dateNow += month(local_time);
  dateNow += "/";
  //  dateNow += year(local_time);
  dateNow += year(local_time) % 100;  // Chỉ lấy 2 chữ số cuối của năm
  spr.drawString(dateNow, 40, 45);

  String timeNow = "";
  if (hour(local_time) < 10) timeNow += "0";
  timeNow += hour(local_time);
  timeNow += ":";
  if (minute(local_time) < 10) timeNow += "0";
  timeNow += minute(local_time);
  spr.loadFont(AA_FONT_XLARGE, LittleFS);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  spr.setTextPadding(tft.textWidth(" 44:44 "));
  spr.drawString(timeNow, 162, 60);
  //  Separator
  drawSeparator(150);
  drawSeparator(249);

  spr.pushSprite(0, 0);

  //  tft.unloadFont();
  /*
      // AUTO DIM BACKLIGHT
      if (hour(local_time) > 20 || hour(local_time) < 6)
        ledcWrite(pwmChannel, 60);
      /*else if (hour(local_time) == 6 && minute(local_time) < 30)
      analogWrite(light,600);
      else if (hour(local_time) == 7 && minute(local_time) > 15)
      ledcWrite(pwmChannel, 20);
      else if (hour(local_time) > 7 && hour(local_time) < 11)
      ledcWrite(pwmChannel, 20);
      else if (hour(local_time) == 11 && minute(local_time) < 40)
      ledcWrite(pwmChannel, 20);
      else if (hour(local_time) > 12 && hour(local_time) < 17)
      ledcWrite(pwmChannel, 20);
      else if (hour(local_time) == 17 && minute(local_time) < 10)
      ledcWrite(pwmChannel, 20);

      else
        ledcWrite(pwmChannel, 255);
  */
}
/***************************************************************************************
**                          Fetch the weather data  and update screen
***************************************************************************************/
// Update the Internet based information and update screen
void updateData(int h, int m) {
  currentWeather = new CurrentWeather();
  HTTPClient http;
  // CurrentWeather Get JSON
  // API endpoint for hourly weather
  String url1h = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/";
  url1h += locationKey;
  url1h += "/today?unitGroup=metric&include=current&key=";
  url1h += apiKey;
  
  bool validData = false;
  int retry = 0;
  
  while (!validData && retry < 5) {
    http.begin(url1h);
    int httpResponseCode1h = http.GET();
    if (httpResponseCode1h > 0) {
      String jsonResponse = http.getString();
      const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(31) + 1024;
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, jsonResponse);
      JsonObject weatherData = doc["currentConditions"];
      if (weatherData["feelslike"].is<float>()) {
        validData = true;
        currentWeather->timeupdate = doc["datetime"].as<String>();
        strncpy(shortKey, apiKey, 6);
        shortKey[6] = '\0';
        jsonglobal = jsonResponse.substring(15, 34) + "/Api: " + shortKey;
        currentWeather->weatherIcon = weatherData["icon"].as<String>();
//        String Icon = String(currentWeather->weatherIcon).substring(0, 24);
//        int weathercon = currentWeather->weatherIcon;
        currentWeather->temperature = weatherData["feelslike"];
        currentWeather->humidity = weatherData["humidity"];
        currentWeather->windDirectionDegrees = weatherData["winddir"];
        currentWeather->windSpeedKmph = weatherData["windgust"];
        currentWeather->rainmm = weatherData["precip"];
        currentWeather->UVindex = weatherData["uvindex"];
        currentWeather->Cloudcover = weatherData["cloudcover"];
        int is_day = (h >= 6 && h < 18) ? 1 : 0;
        currentWeather->isday = is_day;
      }
    }
  http.end();  
  if (!validData) {
    retry++;
    delay(2000);  
  }
  }
  if (!validData) {
    delete currentWeather;
    return;
  }
  GetData = true;
  if (booted)
  {
    drawProgress(100, "Done...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);
  }
  else
  {
    fillSegment(6, 27, 0, 360, 5, TFT_RED);
    fillSegment(6, 27, 0, 360, 6, TFT_BLACK);
  }
  tft.loadFont(AA_FONT_LARGE, LittleFS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  drawCurrentWeather(h, m);

  delete currentWeather;
}


/***************************************************************************************
**                          Draw the current weather
***************************************************************************************/
void drawCurrentWeather(int h, int m) {
  String timeup = "";
  if (h < 10) timeup += "0";
  timeup += h;
  timeup += ":";
  if (m < 10) timeup += "0";
  timeup += m;
  String updatetime = "";
  updatetime += String(currentWeather->timeupdate).substring(11, 16);
  tft.loadFont(AA_FONT_10, LittleFS);
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("44:44 / 44:44"));  // String width + margin
  tft.drawString(jsonglobal + "/" + timeup + "/" + updatetime, 0, 262);
  tft.setTextDatum(BR_DATUM);
  String weathertest = "";
  weathertest = getMeteoconIcon(currentWeather->weatherIcon, currentWeather->Cloudcover, currentWeather->isday);
  ui.drawBmp("/icon/" + weathertest + ".bmp", 0, 50);
  tft.setTextPadding(tft.textWidth("88888"));  // String width + margin
  tft.drawString(String(currentWeather->weatherIcon), 240, 262);  

  String weatherText = "";
  // Weather Text
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("88.8°"));
  tft.drawString(String(currentWeather->temperature, 1) + "/", 110, 60);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.drawString(String(currentWeather->humidity) + "/", 200, 97);

  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth("88.88mm")); // Max string length?
  weatherText = String(currentWeather->rainmm, 2);
  weatherText += "mm";
  tft.drawString(weatherText, 240, 115);

  weatherText = String(currentWeather->windSpeedKmph, 1);
  weatherText += "km/h";
  tft.setTextPadding(tft.textWidth("88,88km/h")); // Max string length?
  tft.drawString(weatherText, 240, 133);

  int windAngle = (currentWeather->windDirectionDegrees + 22.5) / 45;
  if (windAngle > 7) windAngle = 0;
  String wind[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  ui.drawBmp("/wind/" + wind[windAngle] + ".bmp", 110, 95);
  /////////////////////////////////////////////////////////////////
  tft.setTextDatum(BC_DATUM);
  String cloudCover = "";
  cloudCover += currentWeather->Cloudcover;
  cloudCover += "%";
  tft.setTextPadding(tft.textWidth("100%"));
  ui.drawBmp("/icon50/cloud.bmp", 202, 157);
  tft.drawString(cloudCover, 217, 194);

  int UVindex = currentWeather->UVindex;
  ui.drawBmp("/icon50/uv.bmp", 203, 204);
  tft.drawString(String(UVindex), 217, 250);
}


void updateDataMoon(int h, int m) {
  dailyWeather = new DailyWeather();
  HTTPClient http;
  // DailyWeather Get JSON
  // API endpoint for ForeCasts 1 Day
  String url1d = "https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/";
  url1d += locationKey;
  url1d += "/today?unitGroup=metric&include=current&elements=moonrise,moonset,moonphase,sunrise,sunset&key=";
  url1d += apiKey;
  bool validData1 = false;
  int retryM = 0;
  
  while (!validData1 && retryM <5) {  
    http.begin(url1d);
    int httpResponseCode1d = http.GET();
    if (httpResponseCode1d > 0) {
      String jsonResponse1d = http.getString();
    //    Serial.println(jsonResponse1d);
      const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(31) + 1024;
      DynamicJsonDocument doc(capacity);

      DeserializationError error = deserializeJson(doc, jsonResponse1d);
      JsonObject astronomy = doc["days"][0];
      if (astronomy["moonphase"].is<float>()) {
        validData1 = true;     
        dailyWeather->sunrise = astronomy["sunrise"].as<String>();
        dailyWeather->sunset = astronomy["sunset"].as<String>();
        dailyWeather->moonrise = astronomy["moonrise"].as<String>();
        dailyWeather->moonset = astronomy["moonset"].as<String>();
        dailyWeather->moonage = astronomy["moonphase"].as<float>();
   /* Serial.println("🌅 Mặt trời mọc: " + dailyWeather->sunrise);
    Serial.println("🌇 Mặt trời lặn: " + dailyWeather->sunset);
    Serial.println("🌙 Mặt trăng mọc: " + dailyWeather->moonrise);
    Serial.println("🌘 Mặt trăng lặn: " + dailyWeather->moonset); */ 
      }
    }
  http.end();  
  if (!validData1) {
    retryM++;
    delay(2000);  
  }
  }
  if (!validData1) {
    delete dailyWeather;
    return;
  }
  GetMoonPhase = true;
  drawAstronomy();
  delete dailyWeather;
}
/***************************************************************************************
**                          Draw Sun rise/set, Moon, cloud cover and humidity
***************************************************************************************/
void drawAstronomy() {
/*  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);
  int d = day(local_time);
  int m = month(local_time);
  int y = year(local_time);
  int moonage = moonAge(y, m, d);
*/  
  ui.drawBmp("/icon50/sunrise.bmp", 6, 154);
  ui.drawBmp("/icon50/sunset.bmp", 6, 209);
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("88"));
  int moonage = round(dailyWeather->moonage * 29);
  if (moonage < 1) moonage = 1;
  if (moonage > 29) moonage = 29;
  tft.drawString(String(moonage + 1), 92, 250);
  ui.drawBmp("/moon/" + String(moonage) + ".bmp", 52, 152);
  String sunrise12 = String(dailyWeather->sunrise).substring(0, 8);
  String sunrise24 = convertTo24HourFormat(sunrise12);
  String sunset12  = String(dailyWeather->sunset).substring(0, 8);
  String sunset24 = convertTo24HourFormat(sunset12);
  tft.setTextPadding(tft.textWidth("44:44"));
  tft.drawString(String(sunrise24), 24, 194);
  tft.drawString(String(sunset24), 24, 250);
  ui.drawBmp("/icon50/moonrise.bmp", 147, 156);
  ui.drawBmp("/icon50/moonset.bmp", 147, 211);
  String moonrise12 = String(dailyWeather->moonrise).substring(0, 8);
  String moonrise24 = convertTo24HourFormat(moonrise12);
  String moonset12  = String(dailyWeather->moonset).substring(0, 8);
  String moonset24 = convertTo24HourFormat(moonset12);
  tft.drawString(String(moonrise24), 159, 194);
  tft.drawString(String(moonset24), 159, 250);
}
/***************************************************************************************
**                          Get the icon file name from the index number
***************************************************************************************/
const char* getMeteoconIcon(String weatherIcon, int Cloudcover, int isday)
{
  if (isday == 1) {
    if (weatherIcon == "clear-day" && Cloudcover <= 10) return "Clear";
    if (weatherIcon == "clear-day" && Cloudcover > 10) return "Mostly Clear";
    if (weatherIcon == "partly-cloudy-day" && Cloudcover <= 50) return "Partly Cloudy";
    if (weatherIcon == "partly-cloudy-day" && Cloudcover > 50) return "Mostly Cloudy";
    if (weatherIcon == "cloudy") return "Cloudy";
    if (weatherIcon == "fog") return "Fog";
    if (weatherIcon == "wind") return "Wind";
    if (weatherIcon == "rain") return "Rain";
    if (weatherIcon == "showers-day") return "Light Rain";
    if (weatherIcon == "thunder-rain" || weatherIcon == "thunder-showers-day") return "Thunderstorm";
  } else {
    if (weatherIcon == "clear-night" && Cloudcover <= 10) return "nt_Clear";
    if (weatherIcon == "clear-night" && Cloudcover > 10) return "nt_Mostly Clear";
    if (weatherIcon == "partly-cloudy-night" && Cloudcover <= 50) return "nt_Partly Cloudy";
    if (weatherIcon == "partly-cloudy-night" && Cloudcover > 50) return "nt_Mostly Cloudy";
    if (weatherIcon == "cloudy") return "Cloudy";
    if (weatherIcon == "fog") return "Fog";
    if (weatherIcon == "wind") return "Wind";
    if (weatherIcon == "rain") return "Rain";
    if (weatherIcon == "showers-night") return "Light Rain";
    if (weatherIcon == "thunder-rain" || weatherIcon == "thunder-showers-night") return "Thunderstorm";
  }
  return "Unknown";
}
/***************************************************************************************
**                          Update progress bar
***************************************************************************************/
void drawProgress(uint8_t percentage, String text) {
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextPadding(240);
  tft.drawString(text, 120, 260);

  ui.drawProgressBar(10, 269, 240 - 20, 15, percentage, TFT_DARKGREY, TFT_CYAN);

  tft.setTextPadding(0);
  tft.unloadFont();
}

/***************************************************************************************
**                          Draw screen section separator line
***************************************************************************************/
void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

/***************************************************************************************
**                          Determine place to split a line line
***************************************************************************************/
// determine the "space" split point in a long string
int splitIndex(String text)
{
  uint16_t index = 0;
  while ( (text.indexOf(' ', index) >= 0) && ( index <= text.length() / 2 ) ) {
    index = text.indexOf(' ', index) + 1;
  }
  if (index) index--;
  return index;
}

/***************************************************************************************
**                          Right side offset to a character
***************************************************************************************/
// Calculate coord delta from end of text String to start of sub String contained within that text
// Can be used to vertically right align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int rightOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(index));
}

/***************************************************************************************
**                          Left side offset to a character
***************************************************************************************/
// Calculate coord delta from start of text String to start of sub String contained within that text
// Can be used to vertically left align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int leftOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(0, index));
}

/***************************************************************************************
**                          Draw circle segment
***************************************************************************************/
// Draw a segment of a circle, centred on x,y with defined start_angle and subtended sub_angle
// Angles are defined in a clockwise direction with 0 at top
// Segment has radius r and it is plotted in defined colour
// Can be used for pie charts etc, in this sketch it is used for wind direction
#define DEG2RAD 0.0174532925 // Degrees to Radians conversion factor
#define INC 2 // Minimum segment subtended angle and plotting angle increment (in degrees)
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour)
{
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every INC degrees
  for (int i = start_angle; i < start_angle + sub_angle; i += INC) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    tft.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to segment start for next segment
    x1 = x2;
    y1 = y2;
  }
}

String convertTo24HourFormat(String hour12) {
  String hourStr = hour12.substring(0, 2);     // "05"
  String minuteStr = hour12.substring(3, 5);   // "12"
  String period = hour12.substring(6);         // "PM" hoặc "AM"

  int hour = hourStr.toInt();
  int minute = minuteStr.toInt();

  if (period == "PM" && hour != 12) {
    hour += 12;
  } else if (period == "AM" && hour == 12) {
    hour = 0;
  }

  // Định dạng lại theo chuẩn 24h
  String result = (hour < 10 ? "0" : "") + String(hour) + ":" +
                  (minute < 10 ? "0" : "") + String(minute);
  return result;
}
