//***************************************************************************************************
//  texts:        You can add your own language.
//***************************************************************************************************

#define ICON_SUNNY            69             // font u8g2_font_open_iconic_weather
#define ICON_PARTLY_CLOUDY    65             // font u8g2_font_open_iconic_weather
#define ICON_CLOUDY           64             // font u8g2_font_open_iconic_weather
#define ICON_RAIN             67             // font u8g2_font_open_iconic_weather
#define ICON_SNOW             68             // font u8g2_font_open_iconic_weather

#if LANG == 'EN'
  #define TEXT_CITY             "Berlin Friedenau"
  #define TEXT_OUTSIDE          "Balcony:"
  #define TEXT_TODAY            "Today"
  #define TEXT_FORECAST         "Forecast"
  #define TEXT_UPDATED          "updated:"
  #define TEXT_SUN              "Sun:"
  const char* weekdays[] =      {"Su.", "Mo.", "Tu.", "We.", "Th.", "Fr.", "Sa."};
#elif LANG == 'DE'
  #define TEXT_CITY             "Berlin Friedenau"
  #define TEXT_OUTSIDE          "Balkon:"
  #define TEXT_TODAY            "Heute"
  #define TEXT_FORECAST         "Vorschau"
  #define TEXT_UPDATED          "aktual.:"
  #define TEXT_SUN              "Sonne:"
  const char* weekdays[] =      {"So.", "Mo.", "Di.", "Mi.", "Do.", "Fr.", "Sa."};
#endif
