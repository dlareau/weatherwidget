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

void shift_array(float *array, int amount){
  for(int i = 60; i < 130; i++){
    array[i-60] = array[i];
  }
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

void WeatherListener::whitespace(char c) {
}

void WeatherListener::startDocument() {
}

void WeatherListener::key(String key) {
  current_key = key;
  if(current_key == "minutely"){
    current_type = "minutely";
    minute_index = 0;
  } else if(current_key == "hourly"){
    current_type = "hourly";
    hour_index = 0;
  }
}

void WeatherListener::value(String value) {
  if(current_key == "time"){
    int timecode = value.toInt() - 14400;
    if(current_day == 0){
      current_day = timecode - (timecode % 86400);
    }
    if(current_hour == 0){
      current_hour = timecode - (timecode % 3600);
    }
    if(current_type == "minutely"){
      minute_index = (timecode - current_hour) / 60;
      if(minute_index >= 120){
        shift_array(minute_chance, 60);
        shift_array(minute_intensity, 60);
        minute_index -= 60;
      }
    } else if(current_type == "hourly"){
      hour_index = (timecode - current_day) / 3600;
    }
  } else if (current_key == "precipIntensity"){
    if(current_type == "minutely"){
      minute_intensity[minute_index] = value.toFloat();
    } else if(current_type == "hourly"){
      hour_intensity[hour_index] = value.toFloat();
    }
  } else if (current_key == "precipProbability"){
    if(current_type == "minutely"){
      minute_chance[minute_index] = value.toFloat();
    } else if(current_type == "hourly"){
      hour_chance[hour_index] = value.toFloat();
    }
  }
  
  if(current_key == "precipIntensity" && current_type == "minutely"){
    minute_intensity[minute_index] = value.toFloat();
  } else if(current_key == "precipProbability" && current_type == "minutely"){
    minute_chance[minute_index] = value.toFloat();
    minute_index++;
  } else if(current_key == "precipIntensity" && current_type == "hourly"){
    hour_intensity[hour_index] = value.toFloat();
  } else if(current_key == "precipProbability" && current_type == "hourly"){
    hour_chance[hour_index] = value.toFloat();
    hour_index++;
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

