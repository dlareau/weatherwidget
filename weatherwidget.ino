#include "JsonStreamingParser.h"
#include "JsonListener.h"
#include "WeatherParser.h"
#include "parameters.h"
#include <stdio.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define STRIP_PIN D6
#define LOG_OFF 0
#define LOG_INFO 1
#define LOG_DEBUG 2

int log_level = LOG_INFO;

typedef struct toggle_st{
  uint32_t color;
  int led_num;
  int is_active;
} toggle_t;

toggle_t hour_toggle;
toggle_t minute_toggle;

JsonStreamingParser parser;
WeatherListener listener;

const char* host = "api.darksky.net";
const char* api_dest = "/forecast/";
const char* parameters = "?exclude=currently,alerts,flags";
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

int dataIn = D3;
int load = D4;
int clock1 = D5;

// define max7219 registers
byte max7219_reg_noop        = 0x00;
byte max7219_reg_decodeMode  = 0x09;
byte max7219_reg_intensity   = 0x0a;
byte max7219_reg_scanLimit   = 0x0b;
byte max7219_reg_shutdown    = 0x0c;
byte max7219_reg_displayTest = 0x0f;

// RTop, LBottom, DP, Middle, LTop, Top, Bottom, RBottom
int constants[10] = {
  0b11001111,
  0b10000001,
  0b11010110,
  0b10010111,
  0b10011001,
  0b00011111,
  0b01011111,
  0b10000101,
  0b11011111,
  0b10011111
};

void log(String message, int req_level) {
  if(req_level <= log_level){
    Serial.print(message);
  }
}

void log_ln(String message, int req_level) {
  log(message, req_level);
  log("\n", req_level);
}

void putByte(byte data) {
  byte i = 8;
  byte mask;
  while(i > 0) {
    mask = 0x01 << (i - 1);      // get bitmask
    digitalWrite( clock1, LOW);   // tick
    if (data & mask){            // choose bit
      digitalWrite(dataIn, HIGH);// send 1
    }else{
      digitalWrite(dataIn, LOW); // send 0
    }
    digitalWrite(clock1, HIGH);   // tock
    --i;                         // move to lesser bit
  }
}

void maxSingle(byte reg, byte col) {
  //maxSingle is the "easy"  function to use for a single max7219
  digitalWrite(load, LOW);       // begin
  putByte(reg);                  // specify register
  putByte(col);                  // put data
  digitalWrite(load, LOW);       // and load da stuff
  digitalWrite(load,HIGH);
}

void disp_num(int num, int is_top){
  int ldisplay, rdisplay;
  if(is_top){
    ldisplay = 1;
    rdisplay = 3;
  } else {
    ldisplay = 2;
    rdisplay = 5;
  }
  if(num < -9){
    maxSingle(ldisplay,0b01010001);
    maxSingle(rdisplay,0b01010011);
  } else if(num < 0){
    maxSingle(ldisplay,0b00010000);
    maxSingle(rdisplay,constants[(num*(-1))%10]);
  } else {
    maxSingle(ldisplay,constants[(num/10)%10]);
    maxSingle(rdisplay,constants[num%10]);
  }
}

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

  log("\nConnecting to ", LOG_INFO);
  log_ln(ssid, LOG_INFO);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    log(".", LOG_INFO);
  }

  log("\nWiFi connected\nIP address: ", LOG_INFO);
  log_ln(WiFi.localIP().toString(), LOG_INFO);
}

void getWeather(){
  WiFiClientSecure client; 
  char cur_char;
  parser.reset();

  log("Connecting to ", LOG_INFO);
  log_ln(host, LOG_INFO);
  
  if (!client.connect(host, httpsPort)) {
    log_ln("Connection failed", LOG_INFO);
    return;
  }

  log("Requesting URL: ", LOG_INFO);
  log_ln(String("") + api_dest + api_key + "/" + lat_long + parameters, LOG_INFO);
  
  // This will send the request to the server
  client.print(String("GET ") +
      api_dest + api_key + "/" + lat_long + parameters + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n" +
      "Connection: close\r\n\r\n");

  delay(10);

  log_ln("Request sent", LOG_INFO);

  // Read all the lines of the reply from server and print them to Serial
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      log_ln("Recieved headers", LOG_INFO);
      break;
    }
  }
  cur_char = client.read();
  while(cur_char != '\r' && cur_char != 255){
    parser.parse(cur_char);
    cur_char = client.read();
  }

  log_ln("Weather data successfully parsed", LOG_INFO);
}

void setLeds(){
  float *hour_chance = listener.getHourChance();
  float *hour_intensity = listener.getHourIntensity();

  for(int i = 0; i < 24; i++){
    int hue = 240 - int(hour_chance[i]*400);
    int saturation = 255;
    int value = hour_intensity[i]*10000;

    if(hue < 0)
      hue = 0;
    if(value > 255)
      value = 255;

    log_ln("Data:",                           LOG_DEBUG);
    log_ln(String(hour_intensity[i]),         LOG_DEBUG);
    log_ln(String(hour_intensity[i]*10000),   LOG_DEBUG);
    log_ln("HSV:",                            LOG_DEBUG);
    log_ln(String(hue),                       LOG_DEBUG);
    log_ln(String(value),                     LOG_DEBUG);
    uint32_t color = HSVtoRGB(hue, saturation, value);
    strip.setPixelColor(i, color);
  }

  float *minute_chance = listener.getMinuteChance();
  float *minute_intensity = listener.getMinuteIntensity();

  for(int i = 0; i < 20; i++){
    int hue = 240-int(minute_chance[i*3]*400);
    int saturation = 255;
    int value = minute_intensity[i*3]*10000;

    if(hue < 0)
      hue = 0;
    if(value > 255)
      value = 255;

    uint32_t color = HSVtoRGB(hue, saturation, value);
    strip.setPixelColor(44-i, color);
  }

  strip.show();
}

void toggleLed(int led_num, toggle_t *info) {
  if(info->is_active){
    strip.setPixelColor(info->led_num, info->color);
    info->is_active = 0;
  } else {
    int stored_color = strip.getPixelColor(led_num);
    int curr_r = (stored_color >> 16) & 0xFF;
    int curr_g = (stored_color >> 8) & 0xFF;
    int curr_b = stored_color & 0xFF;

    strip.setPixelColor(led_num, 40, 40, 20);
    info->led_num = led_num;
    info->color = stored_color;
    info->is_active = 1;
  }
  return;
}


void setup() {
  pinMode(dataIn, OUTPUT);
  pinMode(clock1,  OUTPUT);
  pinMode(load,   OUTPUT);

  //initiation of the max 7219d
  maxSingle(max7219_reg_scanLimit, 0x07);
  maxSingle(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
  maxSingle(max7219_reg_shutdown, 0x01);    // not in shutdown mode
  maxSingle(max7219_reg_displayTest, 0x00); // no display test
  for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
    maxSingle(e,0);
  }
  maxSingle(max7219_reg_intensity, 0x0f); // intensity, 0x00 to 0x0f

  strip.begin();
  strip.show();
  
  Serial.begin(115200);
  parser.setListener(&listener);

  connectToWifi();

  delay(1000);

}

void loop() {
  getWeather();
  setLeds();
  disp_num((int)listener.getTempHigh(), 0);
  disp_num((int)listener.getTempLow(), 1);
  for(int i = 0; i < 90; i++){
    if(i != 0)
      delay(1000);
    toggleLed(listener.getCurrentHour(), &hour_toggle);
    toggleLed(44-(listener.getCurrentMinute()/3), &minute_toggle);
    strip.show();
  }
}