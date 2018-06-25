#include "WeatherParser.h"
#include "JsonListener.h"

int hour_from_timecode(int timecode){
  int dayless_timecode = timecode % 86400;  // seconds in a day
  int hour = dayless_timecode / 3600; // seconds in an hour
  return hour;
}

int minute_from_timecode(int timecode){
  int hourless_timecode = timecode % 3600; // seconds in an hour
  int minute = hourless_timecode / 60; // seconds in a minute
  return minute;
}

float WeatherListener::getTempHigh(){
  return temp_high;
}

float WeatherListener::getTempLow(){
  return temp_low;
}

float WeatherListener::getTempCurrent(){
  return temp_current;
}
float *WeatherListener::getHourChance(){
  return hour_chance;
}

float *WeatherListener::getMinuteChance(){
  return minute_chance;
}

float *WeatherListener::getHourIntensity(){
  return hour_intensity;
}

float *WeatherListener::getMinuteIntensity(){
  return minute_intensity;
}

int WeatherListener::getCurrentMinute(){
  return minute_from_timecode(start_timecode);
}

int WeatherListener::getCurrentHour(){
  return hour_from_timecode(start_timecode);
}

void WeatherListener::whitespace(char c) {
}

void WeatherListener::startDocument() {
  current_day = 0;
  current_hour = 0;
  start_timecode = 0;
  temp_set = 0;
}

void WeatherListener::key(String key) {
  current_key = key;
  if(current_key == "minutely"){
    current_type = "minutely";
    minute_index = 0;
  } else if(current_key == "hourly"){
    current_type = "hourly";
    hour_index = 0;
  } else if(current_key == "daily"){
    current_type = "daily";
  } else if(current_key == "currently"){
    current_type = "currently";
  }
}

void WeatherListener::value(String value) {
  if(current_type == "daily"){
    if (current_key == "apparentTemperatureHigh" && temp_set != 2){
      Serial.println("Setting High temp");
      temp_low = value.toFloat();
      temp_set++;
    }

    else if (current_key == "apparentTemperatureLow" && temp_set != 2){
      Serial.println("Setting Low temp");
      temp_high = value.toFloat();
      temp_set++;
    }
    return;
  }

  if(current_key == "apparentTemperature" && current_type == "currently"){
    temp_current = value.toFloat();
  }
  
  if(current_key == "time"){
    int timecode = value.toInt() - 18000;
    if(current_day == 0){
      current_day = timecode - (timecode % 86400);
    }
    if(current_hour == 0){
      current_hour = timecode - (timecode % 3600);
    }
    if(start_timecode == 0){
      start_timecode = timecode;
    }
    if(current_type == "minutely"){
      minute_index = (timecode - current_hour) / 60;
    } else if(current_type == "hourly"){
      hour_index = (timecode - current_day) / 3600;
    }
  }

  else if (current_key == "precipIntensity"){
    if(current_type == "minutely"){
      minute_intensity[minute_index] = value.toFloat();
    } else if(current_type == "hourly"){
      hour_intensity[hour_index] = value.toFloat();
    }
  }

  else if (current_key == "precipProbability"){
    if(current_type == "minutely"){
      minute_chance[minute_index] = value.toFloat();
      minute_index++;
    } else if(current_type == "hourly"){
      hour_chance[hour_index] = value.toFloat();
      hour_index++;
    }
  }
}

void WeatherListener::endArray() {
}

void WeatherListener::endObject() {
}

void WeatherListener::endDocument() {
}

void WeatherListener::startArray() {
}

void WeatherListener::startObject() {
}

