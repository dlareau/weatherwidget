#pragma once

#include "JsonListener.h"

class WeatherListener: public JsonListener {
  private:
    String current_key = "";
    String current_type = "";
    int data_state = 0;
    float hour_chance[73];
    float hour_intensity[73];
    float minute_chance[121];
    float minute_intensity[121];
    int hour_index = 0;
    int minute_index = 0;
    int current_day = 0;
    int current_hour = 0;
    int start_timecode = 0;

  public:
    virtual void whitespace(char c);
  
    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();

    float *getHourChance();

    float *getHourIntensity();

    float *getMinuteChance();

    float *getMinuteIntensity();

    int getCurrentMinute();

    int getCurrentHour();
};
