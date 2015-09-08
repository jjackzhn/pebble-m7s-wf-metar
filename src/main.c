
#include "pebble.h"
#include "seven_segment.h"
#include "config.h"
#include "keys.h"
#include "mooncalc.h"
#include "math.h"
#ifdef PBL_SDK_3
  #include "effect_layer.h"
#endif

static Window *s_main_window;
static Layer *s_image_layer_hour_1;
static Layer *s_image_layer_hour_2;
static Layer *s_image_layer_minute_1;
static Layer *s_image_layer_minute_2;
static Layer *s_image_layer_second_1;
static Layer *s_image_layer_second_2;

static int digit_h_1 = 0;
static int digit_h_2 = 0;
static int digit_m_1 = 0;
static int digit_m_2 = 0;
static int digit_s_1 = 0;
static int digit_s_2 = 0;

static GBitmap *background_image;
static BitmapLayer *background_layer;
#ifdef PBL_SDK_3
  static InverterLayer *invert_all_layer;
#endif

static char *sys_locale;



static TextLayer *text_sunrise_layer; 
static TextLayer *text_sunset_layer; 
static TextLayer *battery_runtime_layer;
static TextLayer *connection_layer;

static TextLayer *Date_Layer;
static TextLayer *cwLayer; //calender week

static TextLayer *moonLayer_IMG;
GFont pFontMoon     = 0;
GFont pFontClimacons = 0;

static TextLayer *weather_layer_1_temp; // Temperature
static TextLayer *weather_layer_3_location; // Location Name
static TextLayer *weather_layer_4_last_update; // Time of last data / time since last update
static TextLayer *weather_layer_7_string_1; //configurable, under actual temperature, 2 lines
static TextLayer *weather_layer_7_string_2; //configurable, under moon and battery info, up to 2 infos in one line

static TextLayer *text_TimeZone_layer; //24H/AM/PM and UTC-Offset

static int last_battery_percent = 0;
static int battery_percent_on_exit = 0;
static time_t last_battery_charged_time = 0; //absolute time of last event
static int last_charge_state = 0; //last state of charging on exit (0: discharging; 1: plugged & charging; 2: plugged & full)
static time_t last_battery_period_time = 0; // last duration of charging/discharging period

#ifndef PBL_SDK_3 //only on SDK 2.x
  static InverterLayer *s_battery_layer_fill; //fill battery with an InverterLayer
#else
  static EffectLayer *s_battery_layer_fill; //fill battery with an InverterLayer by an effect_invert_color (my own effect added to effect_layer type)
  //InverterLayer is recreated by including EffektLayer, but it uses the wrong inverting effect.
  static Layer *s_battery_layer_paint_bat;
#endif



// Weather and other global variables:
static time_t phone_last_updated = 0;
static time_t station_data_last_updated = 0;
static char location_name[32];
static int  location_latitude   = (int)(LATITUDE*1E6); //in 1E6
static int  location_longitude  = (int)(LONGITUDE*1E6); //in 1E6
static int  weather_TEMP        = 20; //in degree C
static int  WeatherIcon         = (int)'I'; //sun
//static char weather_string_1[32]; //under actual temp.
//static char weather_string_2[32]; //string under moon/bat
static int  time_UTC_OFFSET     = (int)(TIMEZONE*3600); //in seconds
static char time_ZONE_NAME[10];
static char sun_rise[10] = "--:--";
static char sun_set[10]  = "--:--";
static time_t sun_rise_unix_loc = 0;
static time_t sun_set_unix_loc  = 0;

static char weather_AVWX_string_1[50];
static char weather_AVWX_string_2[50];
static char weather_AVWX_string_3[50];
static char weather_AVWX_string_4[50];
static char weather_AVWX_string_5[50];
/*
static char weather_AVWX_string_6[50];
static char weather_AVWX_string_7[50];
static char weather_AVWX_string_8[50];
static char weather_AVWX_string_9[50];
*/

static int weather_label_1_info_number = 0;
static int weather_label_2_info_number = 0;
static int weather_label_3_info_number = 0;
static int weather_label_4_info_number = 0;
static int weather_label_5_info_number = 4; //this is really needed for the colors (temperature)
static int weather_label_6_info_number = 0;
static int weather_label_7_info_number = 0;
static int weather_label_8_info_number = 0; //cwLayer

//Colors:
GColor textcolor_background;
#ifdef PBL_COLOR
  GColor textcolor_sun;
  GColor textcolor_con;
  GColor textcolor_bat;
  uint8_t textcolor_bat_uint8;
  GColor bkgrcolor_bat;
  uint8_t bkgrcolor_bat_uint8;
  GColor textcolor_date;
  GColor textcolor_cal;
  GColor textcolor_moon;
  GColor textcolor_weather;
  GColor textcolor_location;
  GColor textcolor_last_update;
  GColor textcolor_tz;
#endif
GColor textcolor_clock;
GColor textcolor_seconds;


// Settings variables (App Config):

static int InvertColors = INVERT_COLORS; //used as color profile
static int LightOn = LIGHT_ON;
static int DisplaySeconds = DISPLAY_SECONDS;
static int vibe_on_disconnect = VIBE_ON_DISC;
static int vibe_on_charged_full = VIBE_ON_FULL;
static int vibe_on_hour         = VIBE_ON_HOUR;
static char date_format[20] = DATE_FORMAT;
static int WeatherUpdateInterval = WEATHER_UPDATE_INTERVAL_MINUTE;
static int ShowTimeSinceStationData = 1;
static int TimeZoneFormat = 0;
static char OWM_lang_id[7] = "en";
static int AppFirstStart = 1;
static int MoonPhase = 0;


//runtime variables:
static bool initDone; // e.g. for avoiding "no BT" vibration with initial opening of the watchface
static bool doUpdateWeather;
static int LightIsOn = 0; //saves the state of the background light
static int init_battery_handler = 0;
static char hour_mode_str[4] = "24H";
static int actual_battery_percent = 70;
static time_t time_since_last_update = 0;
static time_t time_since_last_data = 0;
static time_t t_diff_bat = 0;
static int NightMode = 0;
static int WeatherUpdateReceived = 0;

static void set_Date_Layer_size(void);


void print_time(char *s, int size_s, time_t time_diff, int mode){
  
  //mode = 0: 
  //    7 s
  // 1:15 m
  //   10 m
  // 1:05 h
  //   10 h
  // 1:03 d
  //   10 d
  // 3000 d
  
  //mode > 0: 
  //    7 s
  //    1 m
  //   10 m
  //    1 h
  //   10 h
  //    1 d
  //   10 d
  // 3000 d
  
  int days    = time_diff / (24*3600);
  int hours   = (time_diff % (24*3600)) / 3600;
  int minutes = (time_diff % 3600) / 60;
  int seconds = (time_diff % 60);


  if (days == 0){
    if (hours == 0){
      if (minutes == 0){
        snprintf(s, size_s, "%d s", seconds);
      } else {
        if ((minutes < 10) && (mode == 0))
          snprintf(s, size_s, "%d:%02d m", minutes, seconds);
        else
          snprintf(s, size_s, "%d m", minutes);
      }
    } else {
      if ((hours < 10)  && (mode == 0))
        snprintf(s, size_s, "%d:%02d h", hours, minutes);
      else
        snprintf(s, size_s, "%d h", hours);
    }
  } else {
    if ((days < 10) && (mode == 0))
      snprintf(s, size_s, "%d:%02d d", days, hours);
    else
      snprintf(s, size_s, "%d d", days);
  }
}

void replace_degree(char *s, int size_s){
  int i;
  //APP_LOG(APP_LOG_LEVEL_INFO, "size_s = %d", size_s);
  for (i=1; i<size_s; i++){
    //APP_LOG(APP_LOG_LEVEL_INFO, "i = %d   s[i-1] = %c   s[i] = %c", i, s[i-1], s[i]);
    if ((s[i-1] == '_') && (s[i] == '_')){
      s[i-1] = (char)194;
      s[i]   = (char)176;
    }
  }
}




void LoadData(void) {
  //if (persist_exists()) persist_read_string();
  int key = KEY_LOCATION_NAME;
  if (persist_exists(key)) persist_read_string(key, location_name, sizeof(location_name));
  key = KEY_LOCATION_LAT;
  if (persist_exists(key)) location_latitude = persist_read_int(key);
  key = KEY_LOCATION_LON;
  if (persist_exists(key)) location_longitude = persist_read_int(key);
  
  key = KEY_WEATHER_TEMP;
  if (persist_exists(key)) weather_TEMP = persist_read_int(key);
  
  key = KEY_WEATHER_ICON;
  if (persist_exists(key)) WeatherIcon = persist_read_int(key);
  
  key = KEY_WEATHER_UPDATE_INT;
  if (persist_exists(key)) WeatherUpdateInterval = persist_read_int(key);
  
  /*
  key = KEY_WEATHER_STRING_1;
  if (persist_exists(key)) persist_read_string(key, weather_string_1, sizeof(weather_string_1));
  key = KEY_WEATHER_STRING_2;
  if (persist_exists(key)) persist_read_string(key, weather_string_2, sizeof(weather_string_2));
  */
  
  key = KEY_TIME_UTC_OFFSET;
  if (persist_exists(key)) time_UTC_OFFSET = persist_read_int(key);
  
  key = KEY_TIME_LAST_UPDATE;
  if (persist_exists(key)) phone_last_updated = (time_t)(persist_read_int(key));
    
  key = KEY_WEATHER_DATA_TIME;
  if (persist_exists(key)) station_data_last_updated = (time_t)(persist_read_int(key));
  
  key = KEY_TIME_ZONE_NAME;
  if (persist_exists(key)) persist_read_string(key, time_ZONE_NAME, sizeof(time_ZONE_NAME));
  
  key = KEY_SET_MOON_PHASE;
  if (persist_exists(key)) MoonPhase = persist_read_int(key);
  
  key = KEY_WEATHER_AVWX_STRING_1; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_1, sizeof(weather_AVWX_string_1));
  key = KEY_WEATHER_AVWX_STRING_2; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_2, sizeof(weather_AVWX_string_2));
  key = KEY_WEATHER_AVWX_STRING_3; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_3, sizeof(weather_AVWX_string_3));
  key = KEY_WEATHER_AVWX_STRING_4; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_4, sizeof(weather_AVWX_string_4));
  key = KEY_WEATHER_AVWX_STRING_5; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_5, sizeof(weather_AVWX_string_5));
  key = KEY_WEATHER_AVWX_STRING_6; 
  /*
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_6, sizeof(weather_AVWX_string_6));
  key = KEY_WEATHER_AVWX_STRING_7; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_7, sizeof(weather_AVWX_string_7));
  key = KEY_WEATHER_AVWX_STRING_8; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_8, sizeof(weather_AVWX_string_8));
  key = KEY_WEATHER_AVWX_STRING_9; 
  if (persist_exists(key)) persist_read_string(key, weather_AVWX_string_9, sizeof(weather_AVWX_string_9));
  */
  
  key = KEY_BTY_LAST_PERCENT;
  if (persist_exists(key)) last_battery_percent = persist_read_int(key);
  key = KEY_BTY_PERCENT_EXIT;
  if (persist_exists(key)) battery_percent_on_exit = persist_read_int(key);
  key = KEY_BTY_LAST_CHARGED;
  if (persist_exists(key)) last_battery_charged_time = (time_t)(persist_read_int(key));
  
  key = KEY_BTY_LAST_STATE;
  if (persist_exists(key)) last_charge_state = persist_read_int(key);
  key = KEY_BTY_LAST_PERIOD;
  if (persist_exists(key)) last_battery_period_time = (time_t)(persist_read_int(key));
  
  
  init_battery_handler = 1;
  
  key = KEY_SUN_RISE;
  if (persist_exists(key)) persist_read_string(key, sun_rise, sizeof(sun_rise));
  
  key = KEY_SUN_SET;
  if (persist_exists(key)) persist_read_string(key, sun_set, sizeof(sun_set));
  
  
  key = KEY_SUN_RISE_UNIX;
  if (persist_exists(key)) sun_rise_unix_loc = (time_t)(persist_read_int(key));
  
  key = KEY_SUN_SET_UNIX;
  if (persist_exists(key)) sun_set_unix_loc = (time_t)(persist_read_int(key));
  
  
  
  
  key = KEY_SET_INVERT_COLOR;
  if (persist_exists(key)) InvertColors = persist_read_int(key);
  
  key = KEY_SET_LIGHT_ON;
  if (persist_exists(key)) LightOn = persist_read_int(key);
  
  key = KEY_SET_DISPLAY_SEC;
  if (persist_exists(key)) DisplaySeconds = persist_read_int(key);
  
  key = KEY_SET_VIBE_DISC;
  if (persist_exists(key)) vibe_on_disconnect = persist_read_int(key);
  
  key = KEY_SET_VIBE_FULL;
  if (persist_exists(key)) vibe_on_charged_full = persist_read_int(key);
  
  key = KEY_SET_VIBE_HOUR;
  if (persist_exists(key)) vibe_on_hour = persist_read_int(key);
  
  key = KEY_SET_TZ_FORMAT;
  if (persist_exists(key)) TimeZoneFormat = persist_read_int(key);
  
  key = KEY_SET_UPDATE_TIME;
  if (persist_exists(key)) ShowTimeSinceStationData = persist_read_int(key);
  
  key = KEY_SET_DATE_FORMAT;
  if (persist_exists(key)) persist_read_string(key, date_format, sizeof(date_format));
  
  key = KEY_SET_LANG_ID;
  if (persist_exists(key)) persist_read_string(key, OWM_lang_id, sizeof(OWM_lang_id));
  
  
  key = KEY_SET_LABEL_INDEX_1;
  if (persist_exists(key)) weather_label_1_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_2;
  if (persist_exists(key)) weather_label_2_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_3;
  if (persist_exists(key)) weather_label_3_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_4;
  if (persist_exists(key)) weather_label_4_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_5;
  if (persist_exists(key)) weather_label_5_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_6;
  if (persist_exists(key)) weather_label_6_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_7;
  if (persist_exists(key)) weather_label_7_info_number = persist_read_int(key);
  key = KEY_SET_LABEL_INDEX_8;
  if (persist_exists(key)) weather_label_8_info_number = persist_read_int(key);
  
  
  key = KEY_DETECT_FIRST_START;
  if (persist_exists(key)) AppFirstStart = persist_read_int(key); else AppFirstStart = 1;
  
}

void SaveData(void) {
  persist_write_string (KEY_LOCATION_NAME, location_name);
  persist_write_int    (KEY_LOCATION_LAT,  location_latitude);
  persist_write_int    (KEY_LOCATION_LON,  location_longitude);
  
  persist_write_int    (KEY_WEATHER_TEMP,  weather_TEMP);
  persist_write_int    (KEY_WEATHER_ICON,  WeatherIcon);
  persist_write_int    (KEY_WEATHER_UPDATE_INT, WeatherUpdateInterval);
  /*
  persist_write_string (KEY_WEATHER_STRING_1, weather_string_1);
  persist_write_string (KEY_WEATHER_STRING_2, weather_string_2);
  */
  
  persist_write_int    (KEY_TIME_UTC_OFFSET,  time_UTC_OFFSET);
  persist_write_int    (KEY_TIME_LAST_UPDATE,  (int)(phone_last_updated));
  persist_write_int    (KEY_WEATHER_DATA_TIME, (int)station_data_last_updated);
  persist_write_string (KEY_TIME_ZONE_NAME, time_ZONE_NAME);
  persist_write_int    (KEY_SET_MOON_PHASE, MoonPhase);
  
  persist_write_string (KEY_WEATHER_AVWX_STRING_1, weather_AVWX_string_1);
  persist_write_string (KEY_WEATHER_AVWX_STRING_2, weather_AVWX_string_2);
  persist_write_string (KEY_WEATHER_AVWX_STRING_3, weather_AVWX_string_3);
  persist_write_string (KEY_WEATHER_AVWX_STRING_4, weather_AVWX_string_4);
  persist_write_string (KEY_WEATHER_AVWX_STRING_5, weather_AVWX_string_5);
  /*
  persist_write_string (KEY_WEATHER_AVWX_STRING_6, weather_AVWX_string_6);
  persist_write_string (KEY_WEATHER_AVWX_STRING_7, weather_AVWX_string_7);
  persist_write_string (KEY_WEATHER_AVWX_STRING_8, weather_AVWX_string_8);
  persist_write_string (KEY_WEATHER_AVWX_STRING_9, weather_AVWX_string_9);
  */
  
  
  persist_write_int    (KEY_BTY_LAST_PERCENT,  last_battery_percent);
  BatteryChargeState actual_charge = battery_state_service_peek();
  persist_write_int    (KEY_BTY_PERCENT_EXIT,  actual_charge.charge_percent);
  persist_write_int    (KEY_BTY_LAST_CHARGED,  (int)(last_battery_charged_time));
  
  persist_write_int    (KEY_BTY_LAST_STATE,  last_charge_state);
  persist_write_int    (KEY_BTY_LAST_PERIOD,  (int)(last_battery_period_time));
  
  
  persist_write_string (KEY_SUN_RISE, sun_rise);
  persist_write_string (KEY_SUN_SET , sun_set);
  persist_write_int    (KEY_SUN_RISE_UNIX,  (int)sun_rise_unix_loc);
  persist_write_int    (KEY_SUN_SET_UNIX,  (int)sun_set_unix_loc);
  
  persist_write_int(KEY_SET_INVERT_COLOR, InvertColors);
  persist_write_int(KEY_SET_DISPLAY_SEC, DisplaySeconds);
  persist_write_int(KEY_SET_LIGHT_ON, LightOn);
  persist_write_int(KEY_SET_VIBE_DISC, vibe_on_disconnect);
  persist_write_int(KEY_SET_VIBE_FULL, vibe_on_charged_full);
  persist_write_int(KEY_SET_VIBE_HOUR, vibe_on_hour);
  persist_write_int(KEY_SET_TZ_FORMAT, TimeZoneFormat);
  persist_write_string(KEY_SET_DATE_FORMAT, date_format);
  persist_write_int(KEY_SET_UPDATE_TIME, ShowTimeSinceStationData);
  persist_write_string (KEY_SET_LANG_ID, OWM_lang_id);
  
  persist_write_int(KEY_SET_LABEL_INDEX_1, weather_label_1_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_2, weather_label_2_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_3, weather_label_3_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_4, weather_label_4_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_5, weather_label_5_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_6, weather_label_6_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_7, weather_label_7_info_number);
  persist_write_int(KEY_SET_LABEL_INDEX_8, weather_label_8_info_number);
  
}

void DisplayLastUpdated(void) {
  static char last_updated_buffer[10];
  time_t now = time(NULL);
  
  if (ShowTimeSinceStationData){
    time_since_last_data = now - station_data_last_updated;
    //APP_LOG(APP_LOG_LEVEL_INFO, "X: \n(1) = %d\nnow = %d\n(3) = %d\ntime_UTC_OFFSET = %d", 
    //  (int)time_since_last_update, (int)now, (int)station_data_last_updated, (int)time_UTC_OFFSET);
  } else {
    //APP_LOG(APP_LOG_LEVEL_INFO, "THIS SHOULD NOT BE HERE !!!");
  }
  time_since_last_update = now - phone_last_updated;
  
  //display time since last weather update:
  if (time_since_last_update < 10*365*24*3600){ // < 10 years
    if (ShowTimeSinceStationData){
      if (time_since_last_data < 10*365*24*3600){ // < 10 years
        print_time(last_updated_buffer, sizeof(last_updated_buffer), time_since_last_data, 1);
      } else {
        snprintf(last_updated_buffer, sizeof(last_updated_buffer), "--:--");
      }
    } else {
      print_time(last_updated_buffer, sizeof(last_updated_buffer), time_since_last_update, 1);
    }
    //print_time(last_updated_buffer, sizeof(last_updated_buffer), time_since_last_update, 1);
    text_layer_set_text(weather_layer_4_last_update, last_updated_buffer);
    #ifdef PBL_COLOR
      if (ShowTimeSinceStationData){
        if (time_since_last_data >= 2*3600){ // >= 2h
          text_layer_set_text_color(weather_layer_4_last_update, GColorRed);
        } else text_layer_set_text_color(weather_layer_4_last_update, textcolor_last_update);
      } else {
        if (time_since_last_update > WeatherUpdateInterval){
          text_layer_set_text_color(weather_layer_4_last_update, GColorRed);
        } else text_layer_set_text_color(weather_layer_4_last_update, textcolor_last_update);
      }
    #endif
  } else {
    text_layer_set_text(weather_layer_4_last_update, "--:--");
    #ifdef PBL_COLOR
      text_layer_set_text_color(weather_layer_4_last_update, GColorRed);
    #endif
  }
  
  //display battery stats:
  static char battery_buffer_1[20];
  static char battery_buffer_2[10];
  
  if (last_battery_charged_time == 0) last_battery_charged_time = now;
  t_diff_bat = now - last_battery_charged_time;
  if (last_charge_state == 2){
    //if battery if full and plugged, show charge time:
    print_time(battery_buffer_2, sizeof(battery_buffer_2), last_battery_period_time, 0);
  } else if (last_battery_charged_time != 0){
    //if battery if discharging or charging, show the charging/dischraging time:
    print_time(battery_buffer_2, sizeof(battery_buffer_2), t_diff_bat, 0);
  } else {
    strcpy(battery_buffer_2, " ");
  }
  if (last_charge_state == 1){
    snprintf(battery_buffer_1, sizeof(battery_buffer_1), "*%d%%\n%s", actual_battery_percent, battery_buffer_2);
  } else {
    snprintf(battery_buffer_1, sizeof(battery_buffer_1), "%d%%\n%s", actual_battery_percent, battery_buffer_2);
  }
  text_layer_set_text(battery_runtime_layer, battery_buffer_1);
}

void DisplayData(void) {
  #ifdef ITERATE_TEMP
    static char buffer_1[12];
  #endif
  //static char buffer_9[20];
  
  /*
  if (degree_f)
    snprintf(buffer_1, sizeof(buffer_1), "%d°F", (int)(weather_TEMP*1.8+32));
  else
    snprintf(buffer_1, sizeof(buffer_1), "%d°C", weather_TEMP);
  text_layer_set_text(weather_layer_1_temp, buffer_1);
  
  text_layer_set_text(weather_layer_7_string_1, weather_string_1);
  text_layer_set_text(weather_layer_7_string_2, weather_string_2);
  text_layer_set_text(weather_layer_3_location, location_name);
  */
  text_layer_set_text(weather_layer_3_location, weather_AVWX_string_1);
  text_layer_set_text(weather_layer_1_temp,     weather_AVWX_string_2);
  text_layer_set_text(weather_layer_7_string_1, weather_AVWX_string_3);
  text_layer_set_text(weather_layer_7_string_2, weather_AVWX_string_4);
  //if (weather_label_8_info_number != 11){ //if ...=11: display cw-number set in handle_hour_tick(). //TODO: set this index (must be the last one of all possibilities)
    text_layer_set_text(cwLayer,                weather_AVWX_string_5);
  //}
  /*
  text_layer_set_text(weather_layer_3_location, weather_AVWX_string_6);
  text_layer_set_text(weather_layer_3_location, weather_AVWX_string_7);
  text_layer_set_text(weather_layer_3_location, weather_AVWX_string_8);
  text_layer_set_text(weather_layer_3_location, weather_AVWX_string_9);
  */
  
  //#ifdef PBL_SDK_3
    if (clock_is_24h_style()){
      text_layer_set_text(text_sunrise_layer, sun_rise);
      text_layer_set_text(text_sunset_layer, sun_set);
    } else {
      struct tm* sun_time = localtime(&sun_rise_unix_loc);
      static char sun_rise_text[10];
      static char sun_set_text[10];
      strftime(sun_rise_text, sizeof(sun_rise_text), "%I:%M", sun_time);
      text_layer_set_text(text_sunrise_layer, sun_rise_text);
      sun_time = localtime(&sun_set_unix_loc);
      strftime(sun_set_text, sizeof(sun_set_text), "%I:%M", sun_time);
      text_layer_set_text(text_sunset_layer, sun_set_text);
    }
  /*
  #else
    text_layer_set_text(text_sunrise_layer, sun_rise);
    text_layer_set_text(text_sunset_layer, sun_set);
  #endif
  */
  
  #ifdef ITERATE_TEMP
    weather_TEMP++;
    if (weather_TEMP>50) weather_TEMP = -50;
    snprintf(buffer_1, sizeof(buffer_1), "%d°C", weather_TEMP);
    text_layer_set_text(weather_layer_1_temp, buffer_1);
  #endif
  
  #ifdef PBL_COLOR
  if (weather_label_5_info_number == 5) if (InvertColors > 1){
    
    //APP_LOG(APP_LOG_LEVEL_INFO, "t Celsius (color) = %d", weather_TEMP);
    
    if (weather_TEMP >= 40){
      textcolor_weather = GColorRed;  // >= 40°C
    } else if (weather_TEMP >= 28){
      textcolor_weather = GColorOrange; // 28-39°C
    } else if (weather_TEMP >= 26){
      textcolor_weather = GColorChromeYellow;  // 26-27°C
    } else if (weather_TEMP >= 23){
      textcolor_weather = GColorChromeYellow;      // 23-25°C
    } else if (weather_TEMP >= 20){
      textcolor_weather = GColorGreen;          // 20-22°C
    } else if (weather_TEMP >= 18){
      textcolor_weather = GColorMalachite; // 18-19°C
    } else if (weather_TEMP >= 15){
      textcolor_weather = GColorIslamicGreen;   // 15-17°C
    } else if (weather_TEMP >= 10){
      textcolor_weather = GColorJaegerGreen;    // 10-14°C
    } else if (weather_TEMP >= 6){
      textcolor_weather = GColorDarkGray;    // 6-9°C
    } else if (weather_TEMP >= 2){
      textcolor_weather = GColorElectricBlue;           // 2-5°C
    } else if (weather_TEMP >= -1){
      textcolor_weather = GColorCyan;        // -1 - +1°C
    } else if (weather_TEMP >= -10){
      textcolor_weather = GColorVividCerulean;  // -10 - -2°C
    } else if (weather_TEMP >= -20){
      textcolor_weather = GColorPictonBlue;       // -20 - -11°C
    } else if (weather_TEMP >= -30){
      textcolor_weather = GColorBlueMoon;           // -30 - -21°C
    } else {
      textcolor_weather = GColorCobaltBlue;     // < -30°C
    }  
    text_layer_set_text_color(weather_layer_1_temp, textcolor_weather);
  }
  #endif
}




#ifdef PBL_COLOR
static GColor get_weather_icon_color(int nr){
  if (InvertColors < 2) return GColorWhite;
  if (nr < 33) return GColorWhite;
  if (nr > 106) return GColorWhite;
  switch (nr){
    case 33: return GColorVividCerulean; //Cloud
    case 34: return GColorIcterine;
    case 35: return GColorPictonBlue;
    case 36: return GColorBabyBlueEyes;
    case 37: return GColorChromeYellow;
    case 38: return GColorBlueMoon;
    case 39: return GColorCobaltBlue;
    case 40: return GColorChromeYellow;
    case 41: return GColorBlueMoon;
    case 42: return GColorPictonBlue;
    case 43: return GColorOrange;
    case 44: return GColorBlueMoon;
    case 45: return GColorCadetBlue;
    case 46: return GColorRajah;
    case 47: return GColorBlueMoon;
    case 48: return GColorElectricBlue;
    case 49: return GColorPastelYellow;
    case 50: return GColorCadetBlue;
    case 51: return GColorSunsetOrange; //hail (Hagel)
    case 52: return GColorWhite;
    case 53: return GColorWhite;
    case 54: return GColorWhite;
    case 55: return GColorWhite;
    case 56: return GColorWhite;
    case 57: return GColorCeleste; //snow
    case 58: return GColorYellow;
    case 59: return GColorCyan;
    case 60: return GColorLightGray; //fog
    case 61: return GColorPastelYellow;
    case 62: return GColorCadetBlue;
    case 63: return GColorLightGray; //haze (Dunst)
    case 64: return GColorChromeYellow;
    case 65: return GColorCadetBlue;
    case 66: return GColorCeleste; //wind
    case 67: return GColorWhite;
    case 68: return GColorWhite;
    case 69: return GColorWhite;
    case 70: return GColorRed;
    case 71: return GColorOrange;
    case 72: return GColorWhite;
    case 73: return GColorYellow;
    case 74: return GColorOrange;
    case 75: return GColorOrange;
    case 76: return GColorWhite;
    case 77: return GColorWhite;
    case 78: return GColorWhite;
    case 79: return GColorWhite;
    case 80: return GColorWhite;
    case 81: return GColorWhite;
    case 82: return GColorWhite;
    case 83: return GColorWhite;
    case 84: return GColorWhite;
    case 85: return GColorWhite;
    case 86: return GColorWhite;
    case 87: return GColorWhite;
    case 88: return GColorOrange; //tornado
    case 89: return GColorWhite;
    case 90: return GColorBabyBlueEyes; // temp_low
    case 91: return GColorWhite;
    case 92: return GColorWhite;
    case 93: return GColorRed; // temp_high
    case 94: return GColorWhite;
    case 95: return GColorWhite;
    case 96: return GColorWhite;
    case 97: return GColorWhite;
    case 98: return GColorWhite;
    case 99: return GColorWhite;
    case 100: return GColorWhite;
    case 101: return GColorWhite;
    case 102: return GColorWhite;
    case 103: return GColorWhite;
    case 104: return GColorWhite;
    case 105: return GColorWhite;
    case 106: return GColorWhite;
  }
  return GColorBlue;
}
#endif






// Called once per second of DisplaySeconds otherwise once per minute.
static void handle_second_tick(struct tm* current_time, TimeUnits units_changed) {
  
  static struct tm current_time_copy;
  current_time_copy = *current_time;
  
  if (LightOn == 3){
    if (LightIsOn){
      light_enable(0);
      LightIsOn = 0;
    } else {
      light_enable(1);
      LightIsOn = 1;
    }
  } else if (LightOn == 2){
    if (!LightIsOn){
      light_enable(1);
      LightIsOn = 1;
    }
  } else {
    if (LightIsOn){
      light_enable(0);
      LightIsOn = 0;
    }
  }
  
  if (units_changed & HOUR_UNIT){
    if(clock_is_24h_style() == true) {
      digit_h_1 = current_time_copy.tm_hour/10;
      digit_h_2 = current_time_copy.tm_hour%10;
      snprintf(hour_mode_str, sizeof(hour_mode_str), "%s", "24H");
    } else {
      int hour12 = current_time_copy.tm_hour;
      if ((hour12 > 11)){
        snprintf(hour_mode_str, sizeof(hour_mode_str), "%s", "PM");
        hour12-=12;
      } else snprintf(hour_mode_str, sizeof(hour_mode_str), "%s", "AM");
      if (hour12 == 0) hour12 = 12;
      digit_h_1 = hour12/10;
      digit_h_2 = hour12%10;
    }
  }
  
  
  /* //On this position, the new shifted UTC Time changed the date below too when strftime is called.
  static char buffer_9[20];
  if (TimeZoneFormat == 0){
    if (units_changed & MINUTE_UNIT){
      time_t UTC_TIME = time(NULL) - time_UTC_OFFSET;
      if(clock_is_24h_style() == true) {
        strftime(buffer_9, sizeof(buffer_9), "%R Z", localtime(&UTC_TIME)); //attention: in normal watchface this is UTC (not Z)
      } else {
        strftime(buffer_9, sizeof(buffer_9), "%I%M Z", localtime(&UTC_TIME)); //attention: in normal watchface this is UTC (not Z)
      }
    }
  } else if (units_changed & HOUR_UNIT){
    if (TimeZoneFormat == 1){
      if (strlen(time_ZONE_NAME) > 0){
        snprintf(buffer_9, sizeof(buffer_9), "%s", time_ZONE_NAME);
      } else {
        snprintf(buffer_9, sizeof(buffer_9), "UTC+%d", (int)(time_UTC_OFFSET/3600));
      }
    } else if (TimeZoneFormat == 2){
      if (strlen(time_ZONE_NAME) > 0){
        snprintf(buffer_9, sizeof(buffer_9), "%s, %s", hour_mode_str, time_ZONE_NAME);
      } else {
        snprintf(buffer_9, sizeof(buffer_9), "%s, UTC+%d", hour_mode_str, (int)(time_UTC_OFFSET/3600));
      }
    }
  }
  text_layer_set_text(text_TimeZone_layer, buffer_9);
  */
  
  if (units_changed & MINUTE_UNIT){
    digit_m_1 = current_time_copy.tm_min/10;
    digit_m_2 = current_time_copy.tm_min%10;
  }
  
  digit_s_1 = current_time_copy.tm_sec/10;
  digit_s_2 = current_time_copy.tm_sec%10;
  
  static int digit_s_1_old = 10;
  //static int digit_s_2_old = 0;
  static int vibe_hour_old = -1;
  if (units_changed & HOUR_UNIT){
    layer_mark_dirty(s_image_layer_hour_1);
    layer_mark_dirty(s_image_layer_hour_2);
    
    if (vibe_hour_old < 0) vibe_hour_old = current_time_copy.tm_hour;
    if (vibe_on_hour && (vibe_hour_old != current_time_copy.tm_hour)){
      // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
      static const uint32_t const segments[] = { 100 };
      VibePattern pat = {
        .durations = segments,
        .num_segments = ARRAY_LENGTH(segments),
      };
      vibes_enqueue_custom_pattern(pat);
      
      vibe_hour_old = current_time_copy.tm_hour;
    }
  }
  if (units_changed & MINUTE_UNIT){
    layer_mark_dirty(s_image_layer_minute_1);
    layer_mark_dirty(s_image_layer_minute_2);
  }
  if (DisplaySeconds){
    if (digit_s_1_old != digit_s_1){ //should save energy
      layer_mark_dirty(s_image_layer_second_1);
      digit_s_1_old = digit_s_1;
    }
    layer_mark_dirty(s_image_layer_second_2);
  }
  
  
  
  static char date_buffer[20];
  if (units_changed & HOUR_UNIT) {
    strftime(date_buffer, sizeof(date_buffer), /*"%a, %d.%m."*/date_format, &current_time_copy); //this must be before any call to localtime()!!! Otherwise a copy of current_time must be made!
    text_layer_set_text(Date_Layer, date_buffer);
  }
  
  //TxOxDxO: remove:
  /*
  snprintf(date_buffer, sizeof(date_buffer), "wuis = %d", WeatherUpdateInterval);
  text_layer_set_text(Date_Layer, date_buffer);
  */
  
  //static char moon_buffer[7];
  static char moon[] = "m";
  static char weather_icon[] = "I";
  #ifdef PBL_COLOR
    GColor weather_icon_color = GColorWhite;
  #endif
  static int NightModeOld = -1;
  
  //calculate NightMode:
  if (units_changed & MINUTE_UNIT){
  //#ifdef PBL_SDK_3
    
    //sun_set_unix_loc = 1439406657+3600*2;
    //APP_LOG(APP_LOG_LEVEL_INFO, "sun_rise_unix_loc = %d", (int)sun_rise_unix_loc);
    //APP_LOG(APP_LOG_LEVEL_INFO, "sun_set_unix_loc = %d", (int)sun_set_unix_loc);
    struct tm* sun_rise_time = localtime(&sun_rise_unix_loc);
    struct tm  sun_rise_copy = *sun_rise_time;
    struct tm* sun_set_time  = localtime(&sun_set_unix_loc);
    struct tm  sun_set_copy  = *sun_set_time;
    NightMode = 0;
    /*
    APP_LOG(APP_LOG_LEVEL_INFO, "sun_rise_time->tm_hour = %d", sun_rise_copy.tm_hour);
    APP_LOG(APP_LOG_LEVEL_INFO, "sun_rise_time->tm_min = %d", sun_rise_copy.tm_min);
    APP_LOG(APP_LOG_LEVEL_INFO, "sun_set_time->tm_hour = %d", sun_set_copy.tm_hour);
    APP_LOG(APP_LOG_LEVEL_INFO, "sun_set_time->tm_min = %d", sun_set_copy.tm_min);
    APP_LOG(APP_LOG_LEVEL_INFO, "current_time_copy.tm_hour = %d", current_time_copy.tm_hour);
    APP_LOG(APP_LOG_LEVEL_INFO, "current_time_copy.tm_min = %d", current_time_copy.tm_min);
    */
    if (sun_rise_copy.tm_hour > current_time_copy.tm_hour) NightMode = 1;
    if ((sun_rise_copy.tm_hour == current_time_copy.tm_hour) && (sun_rise_copy.tm_min > current_time_copy.tm_min)) NightMode = 1;
    if (sun_set_copy.tm_hour < current_time_copy.tm_hour) NightMode = 1;
    if ((sun_set_copy.tm_hour == current_time_copy.tm_hour) && (sun_set_copy.tm_min <= current_time_copy.tm_min)) NightMode = 1;
  /*
    //if ((current_time_copy.tm_hour > 6) && (current_time_copy.tm_hour < 20) ) NightMode = 0; else NightMode = 1;
  #else
    if ((current_time_copy.tm_hour > 6) && (current_time_copy.tm_hour < 20) ) NightMode = 0; else NightMode = 1;
  #endif
  */
  }
  if (MoonPhase) NightMode = 1; //moon is set to allways displayed
    
  //APP_LOG(APP_LOG_LEVEL_INFO, "NightMode = %d", NightMode);
  
  #ifndef ITERATE_TEMP
    if ((WeatherUpdateReceived) || (units_changed & HOUR_UNIT) || (NightMode != NightModeOld)){
      WeatherUpdateReceived = 0;
      
      if (!NightMode){
        //static int wi_counter = 33;
        text_layer_set_font(moonLayer_IMG, pFontClimacons);
        layer_set_frame(text_layer_get_layer(moonLayer_IMG), GRect(3, 15, 33, 33));
        //layer_mark_dirty(&moonLayer_IMG.layer);  
        
        //wi_counter++; if (wi_counter>106) wi_counter = 33;
        //wi_counter = WeatherIcon;
        weather_icon[0] = (unsigned char)WeatherIcon;//wi_counter;
        text_layer_set_text(moonLayer_IMG, weather_icon);
        
        #ifdef PBL_COLOR
          weather_icon_color = get_weather_icon_color((int)weather_icon[0]);
          text_layer_set_text_color(moonLayer_IMG, weather_icon_color);
        #endif
      }
    }
  #else
    text_layer_set_font(moonLayer_IMG, pFontClimacons);
    layer_set_frame(text_layer_get_layer(moonLayer_IMG), GRect(3, 15, 33, 33));
  
    static int wi_counter = 33;
    wi_counter++; if (wi_counter>106) wi_counter = 33;
    weather_icon[0] = (unsigned char)wi_counter;
    text_layer_set_text(moonLayer_IMG, weather_icon);
    
    #ifdef PBL_COLOR
      weather_icon_color = get_weather_icon_color((int)weather_icon[0]);
      text_layer_set_text_color(moonLayer_IMG, weather_icon_color);
    #endif
  #endif
  
  if (NightMode) if ((units_changed & HOUR_UNIT) || (NightMode != NightModeOld)) {
    // -------------------- Moon_phase
		//static int moonphase_number = 0;
    //moonphase_number += 1;
    int moonphase_number = 0;
    moonphase_number = calc_moonphase_number(location_latitude);
    moon[0] = (unsigned char)(moonphase_char_number(moonphase_number));
    
    
    text_layer_set_font(moonLayer_IMG, pFontMoon);
    layer_set_frame(text_layer_get_layer(moonLayer_IMG), GRect(3, 21, 33, 33));
    text_layer_set_text(moonLayer_IMG, moon);
    #ifdef PBL_COLOR
      weather_icon_color = textcolor_moon;
      text_layer_set_text_color(moonLayer_IMG, weather_icon_color);
    #endif
    
    /*
    snprintf(moon_buffer, sizeof(moon_buffer), "(%d)", moonphase_number);
    //text_layer_set_text(moonLayer, moon_buffer);
    text_layer_set_text(moonLayer, " ");
		//text_layer_set_text(moonLayer, MOONPHASE_NAME_LANGUAGE[moonphase_number]); 
    */
		// -------------------- Moon_phase
  }
  NightModeOld = NightMode;
  
  /*
  if (units_changed & HOUR_UNIT){
    // -------------------- Calendar week  
    if (weather_label_8_info_number == 11){ //TODO: the number 11 and the label to show this info need to be determined.
  	  static char cw_text[] = "XX00";
      if (strcmp("fr_FR", sys_locale) == 0) {
  		  strftime(cw_text, sizeof(cw_text), TRANSLATION_CW_FR, &current_time_copy);
      } else if (strcmp("de_DE", sys_locale) == 0) {
        strftime(cw_text, sizeof(cw_text), TRANSLATION_CW_DE, &current_time_copy);
      } else { //default
        strftime(cw_text, sizeof(cw_text), TRANSLATION_CW_EN, &current_time_copy);
      }
  		text_layer_set_text(cwLayer, cw_text); 
    }
    
		// ------------------- Calendar week 
  }
  */
  
  
  static char buffer_9[20];
  if (TimeZoneFormat == 0){
    if (units_changed & MINUTE_UNIT){
      time_t UTC_TIME_UNIX = time(NULL) - time_UTC_OFFSET;
      struct tm* utc_time = localtime(&UTC_TIME_UNIX);
      if(clock_is_24h_style() == true) {
        strftime(buffer_9, sizeof(buffer_9), "%R Z", utc_time); //attention: in normal watchface this is UTC (not Z)
      } else {
        strftime(buffer_9, sizeof(buffer_9), "%I:%M Z", utc_time); //attention: in normal watchface this is UTC (not Z)
      }
    }
  } else if (units_changed & HOUR_UNIT){
    if (TimeZoneFormat == 1){
      if (strlen(time_ZONE_NAME) > 0){
        snprintf(buffer_9, sizeof(buffer_9), "%s", time_ZONE_NAME);
      } else {
        snprintf(buffer_9, sizeof(buffer_9), "UTC+%d", (int)(time_UTC_OFFSET/3600));
      }
    } else if (TimeZoneFormat == 2){
      if (strlen(time_ZONE_NAME) > 0){
        snprintf(buffer_9, sizeof(buffer_9), "%s, %s", hour_mode_str, time_ZONE_NAME);
      } else {
        snprintf(buffer_9, sizeof(buffer_9), "%s, UTC+%d", hour_mode_str, (int)(time_UTC_OFFSET/3600));
      }
    }
  }
  if (units_changed & MINUTE_UNIT) text_layer_set_text(text_TimeZone_layer, buffer_9);
  
  
  
  
  if (initDone || doUpdateWeather){
    if ((units_changed & MINUTE_UNIT) || doUpdateWeather) {//MINUTE_UNIT, SECOND_UNIT
      if (((int)time_since_last_update >= (WeatherUpdateInterval*60-60)) || doUpdateWeather) { 
        
        doUpdateWeather = false;
        
        // Begin dictionary
        DictionaryIterator *iter;
        app_message_outbox_begin(&iter);
        
        // Add a key-value pair
        dict_write_uint8(iter, 0, 0);
        
        // Send the message!
        app_message_outbox_send();
        
        //APP_LOG(APP_LOG_LEVEL_INFO, "Weather Update requested");
      }
    }
  }
  
  //DisplayData();
  
  /*if (units_changed & MINUTE_UNIT)*/ //DisplayLastUpdated();
  #ifdef ITERATE_TEMP
    DisplayData();
  #endif
  if ((time_since_last_update <= 60) || (t_diff_bat <= 600) || (units_changed & MINUTE_UNIT)){ 
    DisplayLastUpdated(); 
  }
  
  
  
} // ---- end handle_second_tick() ----


static void handle_battery(BatteryChargeState charge_state) {
  
  //charge_state_int = 0: Not plugged (discharging)
  //charge_state_int = 1: plugged and charging
  //charge_state_int = 2: plugged and full
  
  static BatteryChargeState old_charge_state;
  int old_charge_state_int = 0; //not stitic, is calculated from old_charge_state
  
  
  
  if (init_battery_handler){ //call after Load()
    old_charge_state_int = last_charge_state;
  } else {
    if (old_charge_state.is_plugged){
      if (old_charge_state.is_charging) old_charge_state_int = 1; else old_charge_state_int = 2;
    }
  }
  old_charge_state = charge_state;

  if (charge_state.is_plugged){
    if (charge_state.is_charging) last_charge_state = 1; else last_charge_state = 2;
  } else last_charge_state = 0; // = actual charge state
  
  /* (DEBUG)
  static char moon_buffer[10];
  snprintf(moon_buffer, sizeof(moon_buffer), "%d -> %d", old_charge_state_int, last_charge_state);
  text_layer_set_text(moonLayer, moon_buffer);
  */

  if (old_charge_state_int != last_charge_state){
    if (LightOn < 2){
      if (LightOn == 1){
        if (charge_state.is_charging){
          light_enable(0);
          LightIsOn = 0;
        } else {
          light_enable(charge_state.is_plugged);
          if (charge_state.is_plugged) LightIsOn = 1; else LightIsOn = 0;
        }
      }
    }
  }
  
  time_t tdiff = time(NULL) - last_battery_charged_time;
    
  if ((old_charge_state_int == 0) && (last_charge_state == 1)){ //discharging --> charging
    last_battery_period_time = tdiff;
    last_battery_charged_time = time(NULL);
    last_battery_percent = charge_state.charge_percent;
  } else if ((old_charge_state_int == 0) && (last_charge_state == 0)){ //discharging --> discharging
    if ((init_battery_handler) && (battery_percent_on_exit+10 < charge_state.charge_percent)){ //if higher battery percent as on last exit
      last_battery_period_time = tdiff;
      last_battery_charged_time = time(NULL);
      last_battery_percent = charge_state.charge_percent;
    }
  } else if ((old_charge_state_int == 1) && (last_charge_state == 2)){ //charging --> full
    if (tdiff > 10*60) last_battery_period_time = tdiff; else last_battery_period_time += tdiff; //small values are added to old time (because after battery is full, it charges about 6 min. and gets this event a second time)
    last_battery_charged_time = time(NULL);
    last_battery_percent = charge_state.charge_percent;
    
    if (vibe_on_charged_full){
      // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
      static const uint32_t const segments[] = { 500 };
      VibePattern pat = {
        .durations = segments,
        .num_segments = ARRAY_LENGTH(segments),
      };
      vibes_enqueue_custom_pattern(pat);
    }
    
  } else if ((old_charge_state_int == 1) && (last_charge_state == 0)){ //charging --> discharging
    last_battery_period_time = tdiff;
    last_battery_charged_time = time(NULL);
    last_battery_percent = charge_state.charge_percent;
  } else if ((old_charge_state_int == 2) && (last_charge_state == 0)){ //full --> discharging
    //last_battery_period_time = tdiff;
    last_battery_charged_time = time(NULL);
    last_battery_percent = charge_state.charge_percent;
  }
  
  //save last battery percent for displaying in DisplayLastUpdated():
  actual_battery_percent = charge_state.charge_percent;
  
  
  #ifndef PBL_SDK_3 //only on SDK 2.x
    //GRect(41, 21, 38, 11): size of InverterLayer
    layer_set_frame(inverter_layer_get_layer(s_battery_layer_fill), GRect(41, 21, (int)38*actual_battery_percent/100, 11));
    layer_set_hidden(inverter_layer_get_layer(s_battery_layer_fill), false);
  #else
    layer_set_frame(effect_layer_get_layer(s_battery_layer_fill), GRect(41, 21, (int)38*actual_battery_percent/100, 11));
    layer_set_hidden(effect_layer_get_layer(s_battery_layer_fill), false);
    #ifdef PBL_COLOR
      uint8_t variable_color = 0;
      if (actual_battery_percent > 80){
        variable_color = 0b11000100; // 90 % - 100 %
      } else if (actual_battery_percent > 40){
        variable_color = 0b11000100; // 50 % -  80 % 
      } else if (actual_battery_percent > 30){
        variable_color = 0b11111000; // 40 %          light orange (GColorChromeYellow)
      } else if (actual_battery_percent > 20){
        variable_color = 0b11110100; // 30 %          dark orange (GColorOrange)
      } else {
        variable_color = 0b11110000; //  0 % -  20 %  red (GColorRed)
      }
  
      if (InvertColors == 2) if (variable_color == 0b11000100){
        variable_color = 0b11001100;  //light green for black background
      }
      
      if (InvertColors == 2){
        textcolor_bat_uint8 = variable_color;
        bkgrcolor_bat_uint8 = 0b00000000; //black
      } else if (InvertColors == 3) {
        textcolor_bat_uint8 = 0b11111111;
        bkgrcolor_bat_uint8 = variable_color;
      } else {
        textcolor_bat_uint8 = 0b11111111; //white
        bkgrcolor_bat_uint8 = 0b00000000; //black
      }
  
  
  
      GlobalInverterColor = textcolor_bat_uint8 & 0b00111111;
      GlobalBkgColor      = bkgrcolor_bat_uint8 & 0b00111111;
      textcolor_bat       = (GColor8){.argb = textcolor_bat_uint8};
      bkgrcolor_bat       = (GColor8){.argb = bkgrcolor_bat_uint8};
      text_layer_set_text_color(battery_runtime_layer, textcolor_bat);
    #endif
    layer_mark_dirty(s_battery_layer_paint_bat);
  #endif
    
    
  DisplayLastUpdated();
  
  init_battery_handler = 0;
  
}



static void handle_bluetooth(bool connected) {
  if( !connected && initDone)
  {
    if (vibe_on_disconnect){
      // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
      static const uint32_t const segments[] = { 500 };
      VibePattern pat = {
        .durations = segments,
        .num_segments = ARRAY_LENGTH(segments),
      };
      vibes_enqueue_custom_pattern(pat);
    }
  }
  text_layer_set_text(connection_layer, connected ? "Bluetooth" : "---------");
  if (connected && initDone){
    doUpdateWeather = true;
  }
}







static void layer_update_callback_hour_1(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 26, 41), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_clock);
  switch (digit_h_1){
    case 1: 
    seven_segment_paint_1(ctx, 41);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 41);
    break;
    default:
    break;
  }
}

static void layer_update_callback_hour_2(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 26, 41), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_clock);
  switch (digit_h_2){
    case 1: 
    seven_segment_paint_1(ctx, 41);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 41);
    break;
    case 3: 
    seven_segment_paint_3(ctx, 41);
    break;
    case 4: 
    seven_segment_paint_4(ctx, 41);
    break;
    case 5: 
    seven_segment_paint_5(ctx, 41);
    break;
    case 6: 
    seven_segment_paint_6(ctx, 41);
    break;
    case 7: 
    seven_segment_paint_7(ctx, 41);
    break;
    case 8: 
    seven_segment_paint_8(ctx, 41);
    break;
    case 9: 
    seven_segment_paint_9(ctx, 41);
    break;
    case 0: 
    seven_segment_paint_0(ctx, 41);
    break;
    default:
    break;
  }
}
static void layer_update_callback_minute_1(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 26, 41), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_clock);
  switch (digit_m_1){
    case 1: 
    seven_segment_paint_1(ctx, 41);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 41);
    break;
    case 3: 
    seven_segment_paint_3(ctx, 41);
    break;
    case 4: 
    seven_segment_paint_4(ctx, 41);
    break;
    case 5: 
    seven_segment_paint_5(ctx, 41);
    break;
    case 6: 
    seven_segment_paint_6(ctx, 41);
    break;
    case 7: 
    seven_segment_paint_7(ctx, 41);
    break;
    case 8: 
    seven_segment_paint_8(ctx, 41);
    break;
    case 9: 
    seven_segment_paint_9(ctx, 41);
    break;
    case 0: 
    seven_segment_paint_0(ctx, 41);
    break;
    default:
    break;
  }
}
static void layer_update_callback_minute_2(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 26, 41), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_clock);
  switch (digit_m_2){
    case 1: 
    seven_segment_paint_1(ctx, 41);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 41);
    break;
    case 3: 
    seven_segment_paint_3(ctx, 41);
    break;
    case 4: 
    seven_segment_paint_4(ctx, 41);
    break;
    case 5: 
    seven_segment_paint_5(ctx, 41);
    break;
    case 6: 
    seven_segment_paint_6(ctx, 41);
    break;
    case 7: 
    seven_segment_paint_7(ctx, 41);
    break;
    case 8: 
    seven_segment_paint_8(ctx, 41);
    break;
    case 9: 
    seven_segment_paint_9(ctx, 41);
    break;
    case 0: 
    seven_segment_paint_0(ctx, 41);
    break;
    default:
    break;
  }
}

static void layer_update_callback_second_1(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 10, 15), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_seconds);
  switch (digit_s_1){
    case 1: 
    seven_segment_paint_1(ctx, 15);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 15);
    break;
    case 3: 
    seven_segment_paint_3(ctx, 15);
    break;
    case 4: 
    seven_segment_paint_4(ctx, 15);
    break;
    case 5: 
    seven_segment_paint_5(ctx, 15);
    break;
    case 6: 
    seven_segment_paint_6(ctx, 15);
    break;
    case 7: 
    seven_segment_paint_7(ctx, 15);
    break;
    case 8: 
    seven_segment_paint_8(ctx, 15);
    break;
    case 9: 
    seven_segment_paint_9(ctx, 15);
    break;
    case 0: 
    seven_segment_paint_0(ctx, 15);
    break;
    default:
    break;
  }
  if (!DisplaySeconds){
    seven_segment_clear(ctx, 15);
  }
}
static void layer_update_callback_second_2(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 10, 15), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, textcolor_seconds);
  switch (digit_s_2){
    case 1: 
    seven_segment_paint_1(ctx, 15);
    break;
    case 2: 
    seven_segment_paint_2(ctx, 15);
    break;
    case 3: 
    seven_segment_paint_3(ctx, 15);
    break;
    case 4: 
    seven_segment_paint_4(ctx, 15);
    break;
    case 5: 
    seven_segment_paint_5(ctx, 15);
    break;
    case 6: 
    seven_segment_paint_6(ctx, 15);
    break;
    case 7: 
    seven_segment_paint_7(ctx, 15);
    break;
    case 8: 
    seven_segment_paint_8(ctx, 15);
    break;
    case 9: 
    seven_segment_paint_9(ctx, 15);
    break;
    case 0: 
    seven_segment_paint_0(ctx, 15);
    break;
    default:
    break;
  }
  if (!DisplaySeconds){
    seven_segment_clear(ctx, 15);
  }
}

/*
static void layer_update_callback_battery(Layer *layer, GContext* ctx) {
  //clear layer content:
  graphics_context_set_fill_color(ctx, textcolor_background);
  graphics_fill_rect(ctx, GRect(0, 0, 10, 15), 0, GCornerNone);
  //draw:
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, textcolor_seconds);
  graphics_draw_rect(ctx, GRect(0, 0, 43, 15));
}
*/

#ifdef PBL_COLOR
static void layer_update_callback_paint_bat(Layer *layer, GContext* ctx) {
  if (InvertColors == 3){
    graphics_context_set_fill_color(ctx, bkgrcolor_bat);
    graphics_context_set_stroke_color(ctx, bkgrcolor_bat);
    GRect layer_bounds = layer_get_bounds(s_battery_layer_paint_bat);
    graphics_fill_rect(ctx, layer_bounds, 0, 0);
  }
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, textcolor_bat);
  graphics_draw_line(ctx, GPoint( 0+1, 0+2), GPoint(41+1, 0+2));
  graphics_draw_line(ctx, GPoint( 0+1,14+2), GPoint(41+1,14+2));
  graphics_draw_line(ctx, GPoint( 0+1, 0+2), GPoint( 0+1,14+2));
  
  graphics_draw_line(ctx, GPoint(41+1, 0+2), GPoint(41+1, 4+2));
  graphics_draw_line(ctx, GPoint(41+1,10+2), GPoint(41+1,14+2));
  graphics_draw_line(ctx, GPoint(43+1, 4+2), GPoint(43+1,10+2));
  graphics_draw_line(ctx, GPoint(41+1, 4+2), GPoint(43+1, 4+2));
  graphics_draw_line(ctx, GPoint(41+1,10+2), GPoint(43+1,10+2));
}
#endif
  
static void set_Date_Layer_size(void){
  if (DisplaySeconds){
    if (TimeZoneFormat == 2){
      layer_set_frame(text_layer_get_layer(Date_Layer), GRect(72-8, 132, 64+8, 20));
      text_layer_set_text_alignment(Date_Layer, GTextAlignmentLeft);
      //strcpy(date_format, "%a"); //TODO
    } else if (TimeZoneFormat == 0){
      layer_set_frame(text_layer_get_layer(Date_Layer), GRect(50, 132, 64+22, 20));
      text_layer_set_text_alignment(Date_Layer, GTextAlignmentLeft);
    } else if (TimeZoneFormat == 1){
      layer_set_frame(text_layer_get_layer(Date_Layer), GRect(40, 132, 64+32, 20));
      text_layer_set_text_alignment(Date_Layer, GTextAlignmentLeft);
    }
  } else {
    if (TimeZoneFormat == 2){
      layer_set_frame(text_layer_get_layer(Date_Layer), GRect(72-10, 132, 64+10, 20));
      text_layer_set_text_alignment(Date_Layer, GTextAlignmentRight);
    } else {
      layer_set_frame(text_layer_get_layer(Date_Layer), GRect(40, 132, 64+32, 20));
      text_layer_set_text_alignment(Date_Layer, GTextAlignmentRight);
    }
  }
}




static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  
  // Read first item
  Tuple *t = dict_read_first(iterator);
  
  int restart = 0;
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  
  bool Settings_received = false; //determines whether to run handle_second_tick() afterwards.
  bool Settings_change_date_label = false; 

  // For all items
  while(t != NULL) {
    // Which key was received?
    //APP_LOG(APP_LOG_LEVEL_INFO, "received KEY = %d", (int)t->key);
    switch((int)t->key) {
      
    case KEY_WEATHER_AVWX_STRING_1:
      snprintf(weather_AVWX_string_1, sizeof(weather_AVWX_string_1), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_1, sizeof(weather_AVWX_string_1));
      break;
    case KEY_WEATHER_AVWX_STRING_2:
      snprintf(weather_AVWX_string_2, sizeof(weather_AVWX_string_2), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_2, sizeof(weather_AVWX_string_2));
      break;
    case KEY_WEATHER_AVWX_STRING_3:
      snprintf(weather_AVWX_string_3, sizeof(weather_AVWX_string_3), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_3, sizeof(weather_AVWX_string_3));
      break;
    case KEY_WEATHER_AVWX_STRING_4:
      snprintf(weather_AVWX_string_4, sizeof(weather_AVWX_string_4), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_4, sizeof(weather_AVWX_string_4));
      break;
    case KEY_WEATHER_AVWX_STRING_5:
      snprintf(weather_AVWX_string_5, sizeof(weather_AVWX_string_5), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_5, sizeof(weather_AVWX_string_5));
      break;
      /*
    case KEY_WEATHER_AVWX_STRING_6:
      snprintf(weather_AVWX_string_6, sizeof(weather_AVWX_string_6), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_6, sizeof(weather_AVWX_string_6));
      break;
    case KEY_WEATHER_AVWX_STRING_7:
      snprintf(weather_AVWX_string_7, sizeof(weather_AVWX_string_7), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_7, sizeof(weather_AVWX_string_7));
      break;
    case KEY_WEATHER_AVWX_STRING_8:
      snprintf(weather_AVWX_string_8, sizeof(weather_AVWX_string_8), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_8, sizeof(weather_AVWX_string_8));
      break;
    case KEY_WEATHER_AVWX_STRING_9:
      snprintf(weather_AVWX_string_9, sizeof(weather_AVWX_string_9), "%s", t->value->cstring);
      replace_degree(weather_AVWX_string_9, sizeof(weather_AVWX_string_9));
      break;
      */
    case KEY_WEATHER_DATA_TIME:
      station_data_last_updated = (int)t->value->int32;
      /*
      APP_LOG(APP_LOG_LEVEL_INFO, "clock_is_timezone_set() = %d", (int)clock_is_timezone_set());
      time_t xyz = time(NULL);
      APP_LOG(APP_LOG_LEVEL_INFO, "xyz = %d", (int)xyz);
      struct tm* tloc = localtime(&xyz);
      APP_LOG(APP_LOG_LEVEL_INFO, "localtime = %04d-%02d-%02d %02d:%02d:%02d DST=%d", (int)tloc->tm_year, (int)tloc->tm_mon, (int)tloc->tm_mday, (int)tloc->tm_hour, (int)tloc->tm_min, (int)tloc->tm_sec, (int)tloc->tm_isdst);
      #ifdef PBL_SDK_3
        tloc = gmtime(&xyz);
        APP_LOG(APP_LOG_LEVEL_INFO, "gmtime    = %04d-%02d-%02d %02d:%02d:%02d DST=%d", (int)tloc->tm_year, (int)tloc->tm_mon, (int)tloc->tm_mday, (int)tloc->tm_hour, (int)tloc->tm_min, (int)tloc->tm_sec, (int)tloc->tm_isdst);
      #endif
      */
      //APP_LOG(APP_LOG_LEVEL_INFO, "received station_data_last_updated = %d", (int)station_data_last_updated);
      if (!clock_is_timezone_set()){ //convert to the time zone offset of time(NULL)
        station_data_last_updated = station_data_last_updated + time_UTC_OFFSET; //convert to local time only on APLITE
        //APP_LOG(APP_LOG_LEVEL_INFO, "corrected station_data_last_updated = %d", (int)station_data_last_updated);
      }
      break;
      
    case KEY_LOCATION_NAME:
      snprintf(location_name, sizeof(location_name), "%s", t->value->cstring);
      //APP_LOG(APP_LOG_LEVEL_INFO, "KEY_LOCATION_NAME updated last updated time.");
      break;
    case KEY_LOCATION_LAT:
      location_latitude = (int)t->value->int32;
      break;
    case KEY_LOCATION_LON:
      location_longitude = (int)t->value->int32;
      break;
    case KEY_WEATHER_TEMP:
      #ifndef ITERATE_TEMP
        weather_TEMP = (int)t->value->int32;
      #endif
      break;
    case KEY_WEATHER_ICON:
      WeatherIcon = (int)t->value->int32;
      break;
    case KEY_WEATHER_UPDATE_INT: 
      WeatherUpdateInterval = (int)t->value->int32;
      Settings_received = true;
      break;
      /*
    case KEY_WEATHER_STRING_1:
      snprintf(weather_string_1, sizeof(weather_string_1), "%s", t->value->cstring);
      replace_degree(weather_string_1, sizeof(weather_string_1));
      text_layer_set_text(weather_layer_7_string_1, weather_string_1);
      //APP_LOG(APP_LOG_LEVEL_INFO, "weather_string_1 = %s", weather_string_1);
      break;
    case KEY_WEATHER_STRING_2:
      snprintf(weather_string_2, sizeof(weather_string_2), "%s", t->value->cstring);
      replace_degree(weather_string_2, sizeof(weather_string_2));
      text_layer_set_text(weather_layer_7_string_2, weather_string_2);
      //APP_LOG(APP_LOG_LEVEL_INFO, "weather_string_2 = %s", weather_string_2);
      break;
      */
    case KEY_TIME_UTC_OFFSET:
      time_UTC_OFFSET = -(int)t->value->int32;
      phone_last_updated = time(NULL); // save the time the data arrived
      Settings_received = true;
      break;
    case KEY_TIME_ZONE_NAME:
      snprintf(time_ZONE_NAME, sizeof(time_ZONE_NAME), "%s", t->value->cstring);
      Settings_received = true;
      break;
    case KEY_SET_MOON_PHASE:
      MoonPhase = (int)t->value->int32;
      break;
    case KEY_SUN_RISE:
      snprintf(sun_rise, sizeof(sun_rise), "%s", t->value->cstring);
      break;
    case KEY_SUN_SET:
      snprintf(sun_set, sizeof(sun_set), "%s", t->value->cstring);
      break;
    case KEY_SUN_RISE_UNIX:
      sun_rise_unix_loc = (time_t)t->value->int32;
      if (!clock_is_timezone_set()){ //convert to the time zone offset of time(NULL)
        sun_rise_unix_loc += time_UTC_OFFSET;
      }
      break;
    case KEY_SUN_SET_UNIX:
      sun_set_unix_loc = (time_t)t->value->int32;
      if (!clock_is_timezone_set()){ //convert to the time zone offset of time(NULL)
        sun_set_unix_loc += time_UTC_OFFSET;
      }
      break;
      
    case KEY_SET_INVERT_COLOR:
      if ((int32_t)InvertColors != (int)t->value->int32) restart = 1;
      #ifndef PBL_COLOR
        if ((InvertColors == 0) && ((int)t->value->int32) > 1) restart = 0;
      #endif
      InvertColors = (int)t->value->int32;
      #ifndef PBL_COLOR
        //reset all color schemes on aplite platform
        if (InvertColors > 1) InvertColors = 0;
      #endif
      doUpdateWeather = true; //must be done when a configuration was received //TODO: save this and check on startup to avoid not updating after color scheme selection.
      break;
    case KEY_SET_LIGHT_ON:
      LightOn = (int)t->value->int32;
      break;
    case KEY_SET_DISPLAY_SEC:
      DisplaySeconds = (int)t->value->int32;
      Settings_change_date_label = true;
      layer_mark_dirty(s_image_layer_second_1);
      layer_mark_dirty(s_image_layer_second_2);
      tick_timer_service_unsubscribe();
      if (DisplaySeconds)
        tick_timer_service_subscribe(SECOND_UNIT, &handle_second_tick);
      else
        tick_timer_service_subscribe(MINUTE_UNIT, &handle_second_tick);
      break;
      
    case KEY_SET_VIBE_DISC:
      vibe_on_disconnect = (int)t->value->int32;
      break;
    case KEY_SET_VIBE_FULL:
      vibe_on_charged_full = (int)t->value->int32;
      break;
    case KEY_SET_VIBE_HOUR:
      vibe_on_hour = (int)t->value->int32;
      break;
      
    case KEY_SET_TZ_FORMAT:
      TimeZoneFormat = (int)t->value->int32;
      Settings_received = true;
      Settings_change_date_label = true;
      break;
      
    case KEY_SET_UPDATE_TIME:
      ShowTimeSinceStationData = (int)t->value->int32;
      break;
      
    case KEY_SET_DATE_FORMAT:
      snprintf(date_format, sizeof(date_format), "%s", t->value->cstring);
      //APP_LOG(APP_LOG_LEVEL_ERROR, "date_format in watchface = %s", date_format);
      Settings_received = true;
      break;
      
    case KEY_SET_LANG_ID:
      snprintf(OWM_lang_id, sizeof(OWM_lang_id), "%s", t->value->cstring);
      break;
      
      
    case KEY_SET_LABEL_INDEX_1:
      weather_label_1_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_2:
      weather_label_2_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_3:
      weather_label_3_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_4:
      weather_label_4_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_5:
      weather_label_5_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_6:
      weather_label_6_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_7:
      weather_label_7_info_number = (int)t->value->int32;
      break;
    case KEY_SET_LABEL_INDEX_8:
      weather_label_8_info_number = (int)t->value->int32;
      break;
    
    default:
      //APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }
  
  //doUpdateWeather = false;
  
  SaveData();
  
  WeatherUpdateReceived = 1; //this indicates that the weather icon should be displayed if not in night mode.
  
  if (Settings_received) handle_second_tick(tick_time, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT);
  if (Settings_change_date_label) set_Date_Layer_size();
  if (restart) window_stack_pop_all(true); //true means animated = slide out
  
  DisplayData();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}



static void main_window_load(Window *window) {
  
  // Use setlocale() to obtain the system locale for translation
  sys_locale = setlocale(LC_ALL, "");
  LightIsOn = 0;
  
  
  Layer *main_window_layer = window_get_root_layer(s_main_window);
  
  initDone = false;
  
  // --- Load Fonts --- 
  pFontMoon = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MOON_PHASES_SUBSET_24));
  pFontClimacons = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CLIMACONS_32));
  
  //NightMode = 1;
  
  LoadData();
  
  #ifndef PBL_COLOR
    //reset all color schemes on aplite platform
    if (InvertColors > 1) InvertColors = 0;
  #endif
  
  
  // --- Background Image ---
  // --- Background Image ---
  #ifdef PBL_COLOR
    if (InvertColors >= 2){
      switch (InvertColors){
        case 2:
          background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_COLOR_PROFILE_1);
          break;
        case 3:
          background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_COLOR_PROFILE_5);
          break;
        default:
          background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_COLOR_PROFILE_1);
          break;
      }
        
    } else {
      background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_BW);
    }
  #else
    background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_BW);
  #endif
  background_layer = bitmap_layer_create(layer_get_frame(main_window_layer));
  bitmap_layer_set_bitmap(background_layer, background_image);
  #ifdef PBL_COLOR
    bitmap_layer_set_compositing_mode(background_layer, GCompOpAssign);
  #else
    if (InvertColors == 1)
      bitmap_layer_set_compositing_mode(background_layer, GCompOpAssignInverted);
    else
      bitmap_layer_set_compositing_mode(background_layer, GCompOpAssign);
  #endif
  layer_add_child(main_window_layer, bitmap_layer_get_layer(background_layer));
  // --- END ---

  s_image_layer_hour_1 = layer_create(GRect(4, 94, 26, 41));
  layer_set_update_proc(s_image_layer_hour_1, layer_update_callback_hour_1);
  layer_add_child(main_window_layer, s_image_layer_hour_1);
  s_image_layer_hour_2 = layer_create(GRect(37, 94, 26, 41));
  layer_set_update_proc(s_image_layer_hour_2, layer_update_callback_hour_2);
  layer_add_child(main_window_layer, s_image_layer_hour_2);
  
  s_image_layer_minute_1 = layer_create(GRect(80, 94, 26, 41));
  layer_set_update_proc(s_image_layer_minute_1, layer_update_callback_minute_1);
  layer_add_child(main_window_layer, s_image_layer_minute_1);
  s_image_layer_minute_2 = layer_create(GRect(111, 94, 26, 41));
  layer_set_update_proc(s_image_layer_minute_2, layer_update_callback_minute_2);
  layer_add_child(main_window_layer, s_image_layer_minute_2);
  
  s_image_layer_second_1 = layer_create(GRect(113, 137, 10, 15));
  layer_set_update_proc(s_image_layer_second_1, layer_update_callback_second_1);
  layer_add_child(main_window_layer, s_image_layer_second_1);
  s_image_layer_second_2 = layer_create(GRect(126, 137, 10, 15));
  layer_set_update_proc(s_image_layer_second_2, layer_update_callback_second_2);
  layer_add_child(main_window_layer, s_image_layer_second_2);
  
  // --- Create Text-Layers:
  GColor textcolor = GColorWhite;
  
  #ifdef PBL_COLOR
    if (InvertColors >= 2){
      textcolor_background  = GColorFromRGB(0, 0, 0);
      textcolor_sun         = GColorFromRGB(255, 255, 0);   //=GColorYellow //OK
      textcolor_con         = GColorFromRGB(0, 170, 255);   //GColorVividCerulean //connection
      textcolor_bat_uint8   = 0b11110000; //red
      textcolor_bat         = (GColor8){.argb = textcolor_bat_uint8};
      //APP_LOG(APP_LOG_LEVEL_INFO, "textcolor_bat = %d", (int)textcolor_bat); //this does not work
      textcolor_date        = GColorFromRGB(0, 170, 170);   //=GColorTiffanyBlue
      textcolor_cal         = GColorFromRGB(0, 170, 170);   //=GColorTiffanyBlue  //calendar
      textcolor_moon        = GColorFromRGB(255, 255, 255); //OK
      textcolor_weather     = GColorFromRGB(0, 255, 170);   //GColorMediumSpringGreen
      textcolor_location    = GColorFromRGB(255, 170, 0);   //=GColorChromeYellow //OK
      textcolor_last_update = GColorFromRGB(170, 170, 255); //OK
      textcolor_tz          = GColorFromRGB(85, 85, 85); //OK
      textcolor_clock       = GColorFromRGB(0, 255, 0);
      textcolor_seconds     = GColorFromRGB(0, 170, 170);
      
      //override some colors for some profiles:
      if (InvertColors == 3){
        textcolor_background  = GColorFromHEX(0xFFFF55);  //backgound of time and seconds (not of battery. battery is done a little bit down.)
        textcolor_date        = GColorFromRGB(0, 170, 170);   //=GColorTiffanyBlue
        textcolor_cal         = GColorFromRGB(0, 170, 170);   //=GColorTiffanyBlue
        textcolor_weather     = GColorFromRGB(0, 0, 0); //GColorFromRGB(0, 255, 170);   //GColorMediumSpringGreen
        textcolor_location    = textcolor_weather; //GColorFromRGB(170, 85, 0);   //=GColorChromeYellow //OK
        textcolor_last_update = GColorIndigo;
        textcolor_clock       = GColorFromRGB(0, 0, 85);
        textcolor_seconds     = GColorFromRGB(0, 170, 170);
      }
    } else {
      //if (InvertColors == 1){
      //  textcolor = GColorBlack;
      //  textcolor_background = GColorWhite;
      //} else if (InvertColors == 0){
        textcolor = GColorWhite;
        textcolor_background = GColorBlack;
      //}
      textcolor_sun         = textcolor;
      textcolor_con         = textcolor;
      textcolor_bat         = textcolor;
      textcolor_date        = textcolor;
      textcolor_cal         = textcolor;
      textcolor_moon        = textcolor;
      textcolor_weather     = textcolor;
      textcolor_location    = textcolor;
      textcolor_last_update = textcolor;
      textcolor_tz          = textcolor;
      textcolor_clock       = textcolor;
      textcolor_seconds     = textcolor;
    }
  #else  
    if (InvertColors) textcolor = GColorBlack;
    GColor bkgcolor = GColorBlack;
    if (InvertColors) bkgcolor = GColorWhite;
    textcolor_background  = bkgcolor;
    GColor textcolor_sun         = textcolor;
    GColor textcolor_con         = textcolor;
    GColor textcolor_bat         = textcolor;
    GColor textcolor_date        = textcolor;
    GColor textcolor_cal         = textcolor;
    GColor textcolor_moon        = textcolor;
    GColor textcolor_weather     = textcolor;
    GColor textcolor_location    = textcolor;
    GColor textcolor_last_update = textcolor;
    GColor textcolor_tz          = textcolor;
    textcolor_clock       = textcolor;
    textcolor_seconds     = textcolor;
  #endif
    
  // Sunrise Text
  text_sunrise_layer = text_layer_create(GRect(7, 152, 50 /* width */, 30 /* height */)); 
  text_layer_set_text_color(text_sunrise_layer, textcolor_sun);
  text_layer_set_background_color(text_sunrise_layer, GColorClear );
  text_layer_set_font(text_sunrise_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(main_window_layer, text_layer_get_layer(text_sunrise_layer));
  
  // Sunset Text
  text_sunset_layer = text_layer_create(GRect(110, 152, 50 /* width */, 30 /* height */)); 
  text_layer_set_text_color(text_sunset_layer, textcolor_sun);
  text_layer_set_background_color(text_sunset_layer, GColorClear );
  text_layer_set_font(text_sunset_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(main_window_layer, text_layer_get_layer(text_sunset_layer));      
  
  // Connection
  connection_layer = text_layer_create(GRect(47, 152, 50, 34));
  text_layer_set_text_color(connection_layer, textcolor_con);
  text_layer_set_background_color(connection_layer, GColorClear);
  text_layer_set_font(connection_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(connection_layer, GTextAlignmentCenter);
  text_layer_set_text(connection_layer, "----");
  layer_add_child(main_window_layer, text_layer_get_layer(connection_layer));
  
  
  #ifdef PBL_SDK_3
    GlobalInverterColor = textcolor_bat_uint8 & 0b00111111;
    if (InvertColors == 3){
      GlobalBkgColor = 0b00000000;
    }
    
    //s_battery_layer_paint_bat = layer_create(GRect(39, 19, 82-39+1, 33-19+1));
    s_battery_layer_paint_bat = layer_create(GRect(38, 17, 84-38+1, 49-17+1));
    layer_set_update_proc(s_battery_layer_paint_bat, layer_update_callback_paint_bat);
    layer_add_child(main_window_layer, s_battery_layer_paint_bat);
  #endif
  
  // Battery state / runtime:
  battery_runtime_layer = text_layer_create(GRect(40, 15+2, 45, 15+20));
  text_layer_set_text_color(battery_runtime_layer, textcolor_bat);
  text_layer_set_background_color(battery_runtime_layer, GColorClear);
  text_layer_set_font(battery_runtime_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(battery_runtime_layer, GTextAlignmentCenter);
  text_layer_set_text(battery_runtime_layer, "100%\n0:00 d");
  layer_add_child(main_window_layer, text_layer_get_layer(battery_runtime_layer));
  
  #ifndef PBL_SDK_3 //only on SDK 2.x
    //fill battery with an InverterLayer
    s_battery_layer_fill = inverter_layer_create(GRect(41, 21, 38, 11));
    layer_set_hidden(inverter_layer_get_layer(s_battery_layer_fill), true);
    layer_add_child(main_window_layer, inverter_layer_get_layer(s_battery_layer_fill));
  #else //else use effect layer on basalt
    s_battery_layer_fill = effect_layer_create(GRect(41, 21, 38, 11));
    if (InvertColors >= 2){
      effect_layer_add_effect(s_battery_layer_fill, effect_invert_color, (void *)0b00000000); //use global inverter color
    } else {
      effect_layer_add_effect(s_battery_layer_fill, effect_invert_color, (void *)0b00111111);
    }
    layer_set_hidden(effect_layer_get_layer(s_battery_layer_fill), true);
    layer_add_child(main_window_layer, effect_layer_get_layer(s_battery_layer_fill));
  #endif
  
  
  
  //Attention: date and cwLayer are switched in METAR version (and the colors are different!!!):
    
  // Date text
  Date_Layer = text_layer_create(GRect(72, 132, 64 /* width */, 20 /* height */)); 
  text_layer_set_text_color(Date_Layer, textcolor_date);
  text_layer_set_background_color(Date_Layer, GColorClear );
  text_layer_set_font(Date_Layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(Date_Layer, GTextAlignmentLeft);
  layer_add_child(main_window_layer, text_layer_get_layer(Date_Layer));
  set_Date_Layer_size();
  
  // Calendar Week (=weather info)
  cwLayer = text_layer_create(GRect(2, 70, 144-4, 17));
  text_layer_set_text_color(cwLayer, textcolor_weather);
  text_layer_set_background_color(cwLayer, GColorClear );
  text_layer_set_font(cwLayer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(cwLayer, GTextAlignmentLeft);
  layer_add_child(main_window_layer, text_layer_get_layer(cwLayer));
  
  // Moon phase
  moonLayer_IMG = text_layer_create(GRect(3, 18, 33, 33));
  text_layer_set_text_color(moonLayer_IMG, textcolor_moon);
  text_layer_set_background_color(moonLayer_IMG, GColorClear);
  text_layer_set_font(moonLayer_IMG, pFontMoon);
  text_layer_set_text_alignment(moonLayer_IMG, GTextAlignmentCenter);
  layer_add_child(main_window_layer, text_layer_get_layer(moonLayer_IMG));
  
  // --- Weather Layers: ---
  
  // Create temperature Layer
  weather_layer_1_temp = text_layer_create(GRect(50, 10, 94, 30));
  //weather_layer_1_temp = text_layer_create(GRect(0, 10, 144, 30));
  text_layer_set_background_color(weather_layer_1_temp, GColorClear);
  text_layer_set_text_color(weather_layer_1_temp, textcolor_weather);
  text_layer_set_text_alignment(weather_layer_1_temp, GTextAlignmentRight);
  text_layer_set_text(weather_layer_1_temp, "---");
  text_layer_set_font(weather_layer_1_temp, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)); //FONT_KEY_BITHAM_30_BLACK
	layer_add_child(main_window_layer, text_layer_get_layer(weather_layer_1_temp));
  
  // Create location name Layer
  weather_layer_3_location = text_layer_create(GRect(3, -1, 104, 17));
  text_layer_set_background_color(weather_layer_3_location, GColorClear);
  text_layer_set_text_color(weather_layer_3_location, textcolor_location);
  text_layer_set_text_alignment(weather_layer_3_location, GTextAlignmentCenter);
  text_layer_set_text(weather_layer_3_location, "---" /*"Loading Weather ..."*/);
  text_layer_set_font(weather_layer_3_location, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(main_window_layer, text_layer_get_layer(weather_layer_3_location));
  
  // Create last updated Layer
  weather_layer_4_last_update = text_layer_create(GRect(104, -1, 40, 17));
  text_layer_set_background_color(weather_layer_4_last_update, GColorClear);
  text_layer_set_text_color(weather_layer_4_last_update, textcolor_last_update);
  text_layer_set_text_alignment(weather_layer_4_last_update, GTextAlignmentRight);
  text_layer_set_text(weather_layer_4_last_update, "---");
  text_layer_set_font(weather_layer_4_last_update, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(main_window_layer, text_layer_get_layer(weather_layer_4_last_update));
  
  // Create String_1 Layer
  weather_layer_7_string_1 = text_layer_create(GRect(87, 54-15, 144-86-2-1, 30)); //TODO
  text_layer_set_background_color(weather_layer_7_string_1, GColorClear);
  text_layer_set_text_color(weather_layer_7_string_1, textcolor_weather);
  text_layer_set_text_alignment(weather_layer_7_string_1, GTextAlignmentLeft);
  text_layer_set_text(weather_layer_7_string_1, "---\n---");
  text_layer_set_font(weather_layer_7_string_1, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(main_window_layer, text_layer_get_layer(weather_layer_7_string_1));
  
  // Create String_2 Layer
  weather_layer_7_string_2 = text_layer_create(GRect(2, 50, 82, 17)); //TODO
  text_layer_set_background_color(weather_layer_7_string_2, GColorClear);
  text_layer_set_text_color(weather_layer_7_string_2, textcolor_weather);
  text_layer_set_text_alignment(weather_layer_7_string_2, GTextAlignmentLeft);
  text_layer_set_text(weather_layer_7_string_2, "--- / ---");
  text_layer_set_font(weather_layer_7_string_2, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(main_window_layer, text_layer_get_layer(weather_layer_7_string_2));
  
  // Create TimeZone Layer
  text_TimeZone_layer = text_layer_create(GRect(5, 132, 100, 20)); //TODO
  text_layer_set_background_color(text_TimeZone_layer, GColorClear);
  text_layer_set_text_color(text_TimeZone_layer, textcolor_tz);
  text_layer_set_text_alignment(text_TimeZone_layer, GTextAlignmentLeft);
  text_layer_set_text(text_TimeZone_layer, " ");
  text_layer_set_font(text_TimeZone_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(main_window_layer, text_layer_get_layer(text_TimeZone_layer));
    
  // --- END ---
  
  
  
  
  #ifdef PBL_SDK_3 //only on SDK 3.x
    //fill the window with an InverterLayer (must be created as the last element)
    invert_all_layer = inverter_layer_create(GRect(0, 0, 144, 168));
    layer_set_hidden(inverter_layer_get_layer(invert_all_layer), InvertColors != 1);
    layer_add_child(main_window_layer, inverter_layer_get_layer(invert_all_layer));
  #endif
  
  
  DisplayData();
  
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  handle_second_tick(tick_time, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT);
  handle_battery(battery_state_service_peek());
  handle_bluetooth(bluetooth_connection_service_peek());
  
  // --- Register Event Handlers ---
  if (DisplaySeconds)
    tick_timer_service_subscribe(SECOND_UNIT, &handle_second_tick);
  else
    tick_timer_service_subscribe(MINUTE_UNIT, &handle_second_tick);
  battery_state_service_subscribe(&handle_battery);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage
  //APP_LOG(APP_LOG_LEVEL_INFO, "app_message_inbox_size_maximum()  = %d", (int)app_message_inbox_size_maximum());
  //APP_LOG(APP_LOG_LEVEL_INFO, "app_message_outbox_size_maximum() = %d", (int)app_message_outbox_size_maximum());
  #ifdef PBL_SDK_3
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  #else
    //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    app_message_open(400, 10); //in version 12.0, (200, 10) would be ok too. 500 just for security. Maybe 150 would also be OK. But not less!
  #endif
  
  initDone = true;
  doUpdateWeather = false;
  
  if (AppFirstStart){
    AppFirstStart = 0;
    persist_write_int(KEY_DETECT_FIRST_START, AppFirstStart);
    doUpdateWeather = true;
  }
}

static void main_window_unload(Window *window) {
  
  SaveData();
  
  
  // --- Background Image ---
  layer_remove_from_parent(bitmap_layer_get_layer(background_layer));
  bitmap_layer_destroy(background_layer);
  gbitmap_destroy(background_image);
  
  layer_destroy(s_image_layer_hour_1);
  layer_destroy(s_image_layer_hour_2);
  layer_destroy(s_image_layer_minute_1);
  layer_destroy(s_image_layer_minute_2);
  layer_destroy(s_image_layer_second_1);
  layer_destroy(s_image_layer_second_2);
  
  
  #ifndef PBL_SDK_3 //only on SDK 2.x
    inverter_layer_destroy(s_battery_layer_fill);
  #else
    effect_layer_destroy(s_battery_layer_fill);
    layer_destroy(s_battery_layer_paint_bat);
  #endif
  
  
  text_layer_destroy(text_sunrise_layer);
  text_layer_destroy(text_sunset_layer);  
  text_layer_destroy(connection_layer);
  text_layer_destroy(battery_runtime_layer); 
  
  text_layer_destroy(Date_Layer);
  text_layer_destroy(cwLayer);
  
  text_layer_destroy(moonLayer_IMG);
  fonts_unload_custom_font(pFontMoon);
  
  text_layer_destroy(weather_layer_1_temp);
  text_layer_destroy(weather_layer_3_location);
  text_layer_destroy(weather_layer_4_last_update);
  text_layer_destroy(weather_layer_7_string_1);
  text_layer_destroy(weather_layer_7_string_2);
  text_layer_destroy(text_TimeZone_layer);
  
  // --- END ---
  
  #ifdef PBL_SDK_3
    inverter_layer_destroy(invert_all_layer);
  #endif
  
  // --- Event Handlers ---
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
}

static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}