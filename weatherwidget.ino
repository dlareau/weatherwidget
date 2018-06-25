#include "JsonStreamingParser.h"
#include "JsonListener.h"
#include "WeatherParser.h"
#include "parameters.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#define FORMER_SSL_WITH_AXTLS 0

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

extern "C" {
#include "user_interface.h"
}

// OLD
#define DATA_IN_PIN 5
#define LOAD_PIN 4
#define CLOCK_PIN 14
#define STRIP_PIN 12

// NEW
#define DATA_IN_PIN 13
#define LOAD_PIN 12
#define CLOCK_PIN 14
#define STRIP_PIN 4

#define TOP_LEFT   6
#define TOP_CENTER 8
#define TOP_RIGHT  2

#define MID_LEFT   0
#define MID_CENTER 4
#define MID_RIGHT  3

#define BOT_LEFT   1
#define BOT_CENTER 5
#define BOT_RIGHT  7

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
const char* parameters = "?exclude=alerts,flags";
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

Adafruit_NeoPixel strip = Adafruit_NeoPixel(46, STRIP_PIN, NEO_GRB + NEO_KHZ800);

int dataIn = DATA_IN_PIN;
int load = LOAD_PIN;
int clock1 = CLOCK_PIN;

// define max7219 registers
byte max7219_reg_noop        = 0x00;
byte max7219_reg_decodeMode  = 0x09;
byte max7219_reg_intensity   = 0x0a;
byte max7219_reg_scanLimit   = 0x0b;
byte max7219_reg_shutdown    = 0x0c;
byte max7219_reg_displayTest = 0x0f;

// RTop, LBottom, DP, Middle, LTop, Top, Bottom, RBottom
int constants[22] = {
  0b11110110, // 0
  0b11000000, // 1
  0b01010111, // 2
  0b11000111, // 3
  0b11100001, // 4
  0b10100111, // 5
  0b10110111, // 6
  0b11000010, // 7
  0b11110111, // 8
  0b11100011, // 9
  0b11110011, // A
  0b10110101, // b
  0b00010101, // c
  0b11010101, // d
  0b00110111, // E
  0b00110011, // F
  0b00001010, // Middle 1 (16)
  0b00000001, // -        (17)
  0b00100000, // Middle - (18)
  0b10010001, // n        (19)
  0b00100110, // middle n (20)
};

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

void disp_num(int num, int disp){
  int ldisplay, mdisplay, rdisplay;
  if(disp == 0){
    ldisplay = TOP_LEFT;
    mdisplay = TOP_CENTER;
    rdisplay = TOP_RIGHT;
  } else if (disp == 1) {
    ldisplay = MID_LEFT;
    mdisplay = MID_CENTER;
    rdisplay = MID_RIGHT;
  } else if (disp == 2) {
    ldisplay = BOT_LEFT;
    mdisplay = BOT_CENTER;
    rdisplay = BOT_RIGHT;
  } else {
    return;
  }
  
  if(num > 99){
    // Display a leading 1
    if(ldisplay = 0) {
      maxSingle(ldisplay,constants[16]);
    } else {
      maxSingle(ldisplay,constants[1]);
    }
  } else if(num < 0){
    if(ldisplay = 0) {
      maxSingle(ldisplay,constants[18]);
    } else {
      maxSingle(ldisplay,constants[17]);
    }
  } else {
    maxSingle(ldisplay, 0);
  }
  maxSingle(mdisplay,constants[(num/10)%10]);
  maxSingle(rdisplay,constants[num%10]);
}

uint32_t HSVtoRGB(int hue, int sat, int val) {  
  //val = gam[val];
  //sat = 255-gam[255-sat];

  if (sat == 0) { // Acromatic color (gray). Hue doesn't matter.
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
  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  char cur_char;
  int result;
  parser.reset();

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // This will send the request to the server
  client.print(String("GET ") +
      api_dest + api_key + "/" + lat_long + parameters + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n" +
      "Connection: close\r\n\r\n");

  delay(10);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  cur_char = client.read();
  while(cur_char != '\r' && cur_char != 255){
    if(cur_char != 0){
      parser.parse(cur_char);
    }
    if(!client.connected()){
      Serial.println("DISCONNECTED");
      break;
    }
    client.read((uint8_t *)(&cur_char), 1);
  }
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

// Toggles the LED between white and it's last stored value
// Value is stored in info and white is considered active
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
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
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
  parser.setListener(&listener);
  connectToWifi();

  delay(1000);
}

void loop() {
  getWeather();
  setLeds();
  strip.show();
  disp_num((int)listener.getTempHigh(), 0);
  disp_num((int)listener.getTempCurrent(), 1);
  disp_num((int)listener.getTempLow(), 2);
  for(int i = 0; i < 45; i++){
    delay(1000);
    toggleLed(listener.getCurrentHour(), &hour_toggle);
    toggleLed(44-(listener.getCurrentMinute()/3), &minute_toggle);
    strip.show();
  }
}
