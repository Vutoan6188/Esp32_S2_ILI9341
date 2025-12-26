//  Use the OpenWeather library: https://github.com/Bodmer/OpenWeather

//  The weather icons and fonts are in the sketch data folder, press Ctrl+K
//  to view.


//            >>>       IMPORTANT TO PREVENT CRASHES      <<<
//>>>>>>  Set SPIFFS to at least 1.5Mbytes before uploading files  <<<<<<


//                >>>           DON'T FORGET THIS             <<<
//  Upload the fonts and icons to SPIFFS using the "Tools"  "ESP32 Sketch Data Upload"
//  or "ESP8266 Sketch Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/esp8266/arduino-esp8266fs-plugin
//  To add this option follow instructions here for the ESP32:
//  https://github.com/me-no-dev/arduino-esp32fs-plugin

//  Close the IDE and open again to see the new menu option.

// You can change the number of hours and days for the forecast in the
// "User_Setup.h" file inside the OpenWeather library folder.
// By default this is 6 hours (can be up to 48) and 5 days
// (can be up to 8 days = today plus 7 days). This sketch requires
// at least 5 days of forecast. Forecast hours can be set to 1 as
// the hourly forecast data is not used in this sketch.

//////////////////////////////
// Setttings defined below
/*
//man hinh ESP32 PCB test code with LoraSx1278
#define BLYNK_TEMPLATE_ID "TMPL6vzApK9Ae"
#define BLYNK_TEMPLATE_NAME "Station"
#define BLYNK_AUTH_TOKEN "eFvwE8h6OZhmn9NJb9r3XHW0Y22Ia2-E"
*/
/*
//man hinh tren nha Vo
#define BLYNK_TEMPLATE_ID "TMPL6Ia7_YagW"
#define BLYNK_TEMPLATE_NAME "Weather Station II"
#define BLYNK_AUTH_TOKEN "cX9BhGTb0kNNjASSgpq-BAGmZrwg3y2f"
*/
/*
//man hinh phong tro
#define BLYNK_TEMPLATE_ID "TMPL6s2Qtjon_"
#define BLYNK_TEMPLATE_NAME "HOME"
#define BLYNK_AUTH_TOKEN "8QqJ8PCjwVLMTBooMM7_2hrrplhtyXCa"
*/
// Settings WIFI
/*
#define WIFI_SSID      "Goc Que"          //WIFI NHA VO
#define WIFI_PASSWORD  "0345117789"
*/
/*
#define WIFI_SSID      "Mazda Noi Bo"          //WIFI CTY
#define WIFI_PASSWORD  "7979797979"
*/
/*
#define WIFI_SSID      "Vnpt Thuy"        //WIFI NHA CHONG
#define WIFI_PASSWORD  "mnbvClkjhG"
 
/*
#define WIFI_SSID      "ESP32_NAT_Router"        //  Repeater
#define WIFI_PASSWORD  "Ht210119"
*/

#define TIMEZONE VI // See NTP_Time.h tab for other "Zone references", UK, usMT etc

// Update every 15 minutes, up to 1000 request per day are free (viz average of ~40 per hour)
//const int UPDATE_INTERVAL_SECS = 30 * 60UL; // 30 minutes
// Pins for the TFT interface are defined in the User_Config.h file inside the TFT_eSPI library

// Settings API Accuweather Account
//const char* apiKey = "xxx";
//const char* apiKey = "m5qDSrtuKItuxhB3sqs48tXTIWRPfa2I";  //vutoan6188@gmail.com //NHA VO
//const char* apiKey = "DtGrpTbj1AhQgq7RT4h2y43vlbWj5Tgc";  //langthang6188@gmail.com //nha Ong Noi
//const char* apiKey = "4KKRSO1IZQvULW2orPzAc9FlVnIuzpFm";  //minix126vovankiet@gmail.com// test EPS32 S2
//const char* apiKey = "CL1bY6j6RqU1CzhNHYn7CVYUEbHIjxih";  //esp32homecontrol@gmail.com
//const char* apiKey = "jspHK5xf9gpvVPCdw59nNo6ENCoBHVmQ";  //sanman101122@gmail.com
//const char* apiKey = "eKNJoG2M5NuvUfj62Wai8D9oLzP0K7wG";  //phanmytrang1973@gmail.com// NHA VO

//Settings Location
//const char* locationKey = "352955"; // BMT //Location key for your desired location
//const char* locationKey = "352966";//352966 Cuor Dang Nong Trai

// Change the labels to your language here:
/*const char sunStr[]        = "Sun";
const char cloudStr[]      = "Cloud";
const char uvStr[]   = "UV";
*/
// End of user settings
//////////////////////////////
