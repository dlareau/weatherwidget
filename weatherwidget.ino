#include "JsonStreamingParser.h"
#include "JsonListener.h"
#include "WeatherParser.h"
#include "parameters.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define STRIP_PIN D6

JsonStreamingParser parser;
WeatherListener listener;

const char* host = "api.darksky.net";
const char* api_dest = "/forecast/";
const char* parameters = "?exclude=currently,daily,alerts,flags";
const int httpsPort = 443;

int gam[] = {
  0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
  6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
  8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
  11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
  15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
  20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
  27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
  36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
  48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
  83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
  110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
  146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
  193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, STRIP_PIN, NEO_GRBW + NEO_KHZ800);

uint32_t HSVtoRGB(int hue, int sat, int val) {  
  val = gam[val];
  sat = 255-gam[255-sat];

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    return strip.Color(val, val, val);
  }

  int base = ((255 - sat) * val) >> 8;

  switch(hue/60) {
    case 0:
      return strip.Color(val, (((val-base)*hue)/60)+base, base);
    case 1:
      return strip.Color((((val-base)*(60-(hue%60)))/60)+base, val, base);
    case 2:
      return strip.Color(base, val, (((val-base)*(hue%60))/60)+base);
    case 3:
      return strip.Color(base, (((val-base)*(60-(hue%60)))/60)+base, val);
    case 4:
      return strip.Color((((val-base)*(hue%60))/60)+base, base, val);
    case 5:
      return strip.Color(val, base, (((val-base)*(60-(hue%60)))/60)+base);
  } 
}

void connectToWifi(){
  delay(10);
  Serial.println();

  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void getWeather(){
  WiFiClientSecure client; 
  char cur_char;

  Serial.print("Connecting to ");
  Serial.println(host);
  
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") +
      api_dest + api_key + "/" + lat_long + parameters + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n" +
      "Connection: close\r\n\r\n");

  delay(10);

  parser.setListener(&listener);

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("Request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Recieved headers");
      break;
    }
  }
  cur_char = client.read();
  while(cur_char != '\r' && cur_char != 255){
    parser.parse(cur_char);
    cur_char = client.read();
  }

  Serial.println("Weather data successfully parsed");
}

void setLeds(){
  float *hour_chance = listener.getHourChance();
  float *hour_intensity = listener.getHourIntensity();

  for(int i = 0; i < 24; i++){
    int hue = 240-int(hour_chance[i+24]*400);
    int saturation = 255;
    int value = hour_intensity[i+24]*10000;

    if(hue < 0)
      hue = 0;
    if(value > 255)
      value = 255;

    uint32_t color = HSVtoRGB(hue, saturation, value);
    strip.setPixelColor(i, color);
    /* Serial.print(hour_chance[i]);
    Serial.print(" ");
    Serial.println(hour_intensity[i]); */
  }

  float *minute_chance = listener.getMinuteChance();
  float *minute_intensity = listener.getMinuteIntensity();

  for(int i = 0; i < 20; i++){
    int hue = 100+i*3; //240-int(minute_chance[i*3]*400);
    int saturation = 255;
    int value = 100+i*3; //minute_intensity[i*3]*10000;

    if(hue < 0)
      hue = 0;
    if(value > 255)
      value = 255;

    uint32_t color = HSVtoRGB(hue, saturation, value);
    strip.setPixelColor(44-i, color);
    /* Serial.print(hour_chance[i]);
    Serial.print(" ");
    Serial.println(hour_intensity[i]); */
  }

  strip.show();
}

void toggleLed(int led_num) {
  static int off_led_num = 0;
  static int is_off = 0;
  static uint32_t stored_color = 0;

  if(is_off == 1){
    strip.setPixelColor(off_led_num, stored_color);
    is_off = 0;
  } else {
    stored_color = strip.getPixelColor(led_num);
    is_off = 1;
    off_led_num = led_num;
    int curr_r = (stored_color >> 16) & 0xFF;
    int curr_g = (stored_color >> 8) & 0xFF;
    int curr_b = stored_color & 0xFF;
    strip.setPixelColor(off_led_num, curr_r, curr_g, curr_b, 20);
  }
  strip.show();
}


void setup() {
  strip.begin();
  strip.show();
  
  Serial.begin(115200);

  connectToWifi();

  delay(1000);
}

void loop() {
  getWeather();
  setLeds();
  for(int i = 0; i < 15; i++){
    if(i != 0)
      delay(1000);
    toggleLed(listener.getCurrentHour());
    toggleLed((listener.getCurrentMinute()/3)+25);
  }
}

