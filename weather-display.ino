//***************************************************************************************************
//  weather-display:            ePaper display for weather information.
// 
//                              By Ingo Hoffmann.
//***************************************************************************************************
//
//  Hardware components:
//  Board:                      LOLIN D32
//
//  Libraries used:
//    WiFi                      wifi connection
//    esp_wifi                  enables and disables wifi to save energy
//    PubSubClient              connect to MQTT
//    GxEPD2_3C                 
//    GxEPD2                    
//    GxEPD_EPD                 
//    GxEPD_GFX                 
//    U8g2_for_Adafruit_GFX     
//    u8g2_fonts                
//    OpenWeatherMapCurrent     
//    OpenWeatherMapForecast    by Daniel Eichhorn
//                              added 2 lines to AerisObservations.h and AerisForecasts.h
//                              as suggested by bill-orange here:
//                              https://github.com/ThingPulse/esp8266-weather-station/issues/71
//    ezTime                          
//
// Mapping for WaveShare shield:
// mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
// NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
//
//  Dev history:
//    08.10.2019, IH            First set-up, using WaveShare e-Paper ESP8266 Driver Board
//    13.11.2019, IH            switched to Lolin32 ESP32 board with GooDisplay adapter to use deep sleep,
//                              switched to ezTime
//    11.08.2020, IH            recompiled with updated libraries
//
//***************************************************************************************************

#define VERSION                 "1.3"   // 11.08.20

// libraries
#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <GxEPD2_3C.h>
#include <GxEPD2.h>
#include <GxEPD2_EPD.h>
#include <GxEPD2_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <u8g2_fonts.h>
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include <ezTime.h>

// project files
#include "secrets.h"
#include "settings.h"
#include "texts.h"

//***************************************************************************************************
//  Global consts
//***************************************************************************************************
#define MAX_FORECASTS 20

//***************************************************************************************************
//  Global data
//***************************************************************************************************
// Waveshare 4.2" tricolor display
GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display(GxEPD2_420c(SS, 17, 16, 4));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
OpenWeatherMapCurrentData conditions;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
Timezone myTZ;
uint8_t foundForecasts = 0;
bool wifiConnected = false;
bool mqttConnected = false;
bool newTemp = false;
bool newHumidity = false;
bool newPressure = false;
bool newVoltage = false;
float outdoorSensorTemp = 0.0;
float outdoorSensorHumidity = 0.0;
float outdoorSensorPressure = 0.0;
float outdoorSensorVoltage = 0.0;

//***************************************************************************************************
//  SETUP
//***************************************************************************************************
void setup() {
  Serial.begin(115200);

  // setup display
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  u8g2Fonts.begin(display);                   // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                   // use u8g2 transparent mode (this is default)
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

  Serial.println();
  Serial.println();
  Serial.println("Waking up");

  // setup wifi
  esp_wifi_start();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);  
  // connect
  connectToWifi();
  if(!wifiConnected) {
    Serial.println("No WiFi! Restarting ...");
    delay(1000);
    ESP.restart();
  }

  // config time
  getNetworkTime();

  // setup MQTT
  mqttClient.setServer(SECRET_MQTT_BROKER, SECRET_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

//***************************************************************************************************
//  LOOP
//***************************************************************************************************
void loop() {
  if(!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  // wait for all outdoor sensor data being available before refreshing screen once
  if(newTemp && newHumidity && newPressure && newVoltage) {
    newTemp = false;
    newHumidity = false;
    newPressure = false;
    newVoltage = false;
    Serial.println("New outdoor sensor data available");
    updateOWMData();
    Serial.println("OWM data received");
    do {
      // clear screen
      display.fillScreen(GxEPD_WHITE);
      drawStaticElements();
      showUpdateTime();
      showCurrentWeather();
      showForecastWeather();
      showForecastTempChart();
      showOutdoorSensorData();
    } while (display.nextPage());
    
    int err = esp_wifi_stop();
    esp_sleep_enable_timer_wakeup(updateIntervall * 60e6);
    Serial.println("Going to sleep");
    esp_deep_sleep_start();
  }
}

//***************************************************************************************************
//  connectToWifi
//  
//***************************************************************************************************
void connectToWifi() {
  int wifiRetries = 0;

  while ((WiFi.status() != WL_CONNECTED) && wifiRetries < 8) {
    delay(800);
    wifiRetries++;
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if(wifiConnected) {
    Serial.println("WiFi connected");
  } else {
    Serial.println("WiFi not connected");
  }
}

//***************************************************************************************************
//  connectToMQTT
//  
//***************************************************************************************************
void connectToMQTT() {
  int mqttRetries = 0;

  while(!mqttClient.connected() && mqttRetries < 5) {
    if(mqttClient.connect(mqttClientID, SECRET_MQTT_USER, SECRET_MQTT_PASSWORD)) {
      mqttSubscribeToTopics();
    } else {
      delay(1000);
      mqttRetries++;
    }
  }
  mqttConnected = mqttClient.connected();
  if(mqttConnected) {
    Serial.println("MQTT connected");
  } else {
    Serial.println("MQTT not connected");
  }
}

//***************************************************************************************************
//  mqttSubscribeToTopics
//  to avoid feedback loops we subscribe to command messages and send status messages
//***************************************************************************************************
void mqttSubscribeToTopics() {
  mqttClient.subscribe(mqttTopicTemp);
  mqttClient.subscribe(mqttTopicHumidity);
  mqttClient.subscribe(mqttTopicPressure);
  mqttClient.subscribe(mqttTopicVoltage);
}

//***************************************************************************************************
//  mqttCallback
//  
//***************************************************************************************************
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String payloadStr = (char*) payload;

  Serial.print("MQTT message received: "); 
  Serial.print(topic); 
  Serial.print(", payload = ");
  Serial.println(payloadStr);
  
  if(strcmp(topic, mqttTopicTemp) == 0) {
    outdoorSensorTemp = payloadStr.toFloat();
    newTemp = true;
  } else if(strcmp(topic, mqttTopicHumidity) == 0) {
    outdoorSensorHumidity = payloadStr.toFloat();
    newHumidity = true;
  } else if(strcmp(topic, mqttTopicPressure) == 0) {
    outdoorSensorPressure = payloadStr.toFloat();
    newPressure = true;
  } else if(strcmp(topic, mqttTopicVoltage) == 0) {
    outdoorSensorVoltage = payloadStr.toFloat();
    newVoltage = true;
  } else {
    // unrecognized command
    Serial.println("unrecognized MQTT message"); 
  }
}

//***************************************************************************************************
//  updateOWMData
//  
//***************************************************************************************************
void updateOWMData() {
  OpenWeatherMapCurrent *conditionsClient = new OpenWeatherMapCurrent();
  conditionsClient->setLanguage(owmLanguage);
  conditionsClient->setMetric(owmIsMetric);
  conditionsClient->updateCurrentById(&conditions, SECRET_OWM_APP_ID, owmLocationID);
  delete conditionsClient;
  conditionsClient = nullptr;

  OpenWeatherMapForecast *forecastsClient = new OpenWeatherMapForecast();
  forecastsClient->setLanguage(owmLanguage);
  forecastsClient->setMetric(owmIsMetric);
  foundForecasts = forecastsClient->updateForecastsById(forecasts, SECRET_OWM_APP_ID, owmLocationID, 
                                                        MAX_FORECASTS);
  delete forecastsClient;
  forecastsClient = nullptr;
}

//***************************************************************************************************
//  drawStaticElements
//  
//***************************************************************************************************
void drawStaticElements() {
  display.drawFastHLine(0, headerH + 0, 400, GxEPD_BLACK);
  display.drawFastHLine(0, headerH + 1, 400, GxEPD_BLACK);
  display.drawFastHLine(0, headerH + 2, 400, GxEPD_BLACK);

  display.drawFastHLine(0, headerH + todayH + 3, 400, GxEPD_BLACK);
  display.drawFastHLine(0, headerH + todayH + 4, 400, GxEPD_BLACK);

  for(int i = 0; i < 5; i++) {
    display.drawFastVLine(nowW + 2 + (i * forecastW), 
                          headerH + todayHeaderH + 3 + tableSeparation, 
                          todayH - todayHeaderH - tableSeparation, GxEPD_BLACK);
  }

  display.drawFastHLine(tempW + 3, headerH + todayH + forecastHeaderH + daysH + 5, 
                        400 - tempW - 2, GxEPD_BLACK);

  display.drawFastVLine(tempW + 2, headerH + todayH + forecastHeaderH + 5, 
                        300 - headerH - todayH - forecastHeaderH, GxEPD_BLACK);

  u8g2Fonts.setFontDirection(0);              // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);

    u8g2Fonts.setCursor(cityX, cityY);
    u8g2Fonts.print(TEXT_CITY);

    u8g2Fonts.setCursor(todayX, todayY);
    u8g2Fonts.print(TEXT_TODAY);

    u8g2Fonts.setCursor(forecastX, forecastY);
    u8g2Fonts.print(TEXT_FORECAST);

  u8g2Fonts.setFont(u8g2_font_helvR12_tf);
    u8g2Fonts.setCursor(outsideX, outsideY);
    u8g2Fonts.print(TEXT_OUTSIDE);

    u8g2Fonts.setCursor(sunX, sunY);
    u8g2Fonts.print(TEXT_SUN);

    u8g2Fonts.setCursor(updatedX, updatedY);
    u8g2Fonts.print(TEXT_UPDATED);
}

//***************************************************************************************************
//  getNetworkTime
//  get network time according to time zone, needs wifi connection
//***************************************************************************************************
void getNetworkTime() {
  waitForSync();                    // Wait for ezTime to get its time synchronized
  myTZ.setLocation(F(timezone));
  setInterval(60 * 60 * 24);        // set NTP polling interval to daily
  setDebug(NONE);                   // NONE = set ezTime to quiet
  Serial.println("Time is now " + myTZ.dateTime("H:i"));
}

//***************************************************************************************************
//  showUpdateTime
//  
//***************************************************************************************************
void showUpdateTime() {
  u8g2Fonts.setFont(u8g2_font_helvR12_tf);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setCursor(updatedTimeX, updatedTimeY);
  u8g2Fonts.print(UTC.dateTime(conditions.observationTime - myTZ.getOffset() * 60, "H:i"));
}

//***************************************************************************************************
//  showCurrentWeather
//  
//***************************************************************************************************
void showCurrentWeather() {
  u8g2Fonts.setFont(u8g2_font_helvR12_tf);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setCursor(conditionX, conditionY);
  u8g2Fonts.print(conditions.description);
  u8g2Fonts.setCursor(daylightX, daylightY);
  u8g2Fonts.print(UTC.dateTime(conditions.sunrise - myTZ.getOffset() * 60, "H:i") + " - " + 
                  UTC.dateTime(conditions.sunset - myTZ.getOffset() * 60, "H:i"));
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  u8g2Fonts.setCursor(todayNowX + 12, todayNowTempY);
  if(conditions.temp <= friendlyTemp) {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
  }
  u8g2Fonts.print(String(conditions.temp, 0) + "°");          // no C or F to save space
  u8g2Fonts.setFont(u8g2_font_open_iconic_weather_6x_t);
  drawWeatherIcon(conditions.icon, todayNowX, todayNowIconY);
}

//***************************************************************************************************
//  showForecastWeather
//  
//***************************************************************************************************
void showForecastWeather() {
  for(int i = 0; i < 5; i++) {
    u8g2Fonts.setFont(u8g2_font_helvR12_tf);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setCursor(todayTableFirstX + (i * forecastW) + 4, todayTableHoursY);
    u8g2Fonts.print(UTC.dateTime(forecasts[i].observationTime - myTZ.getOffset() * 60, "H") + "h");
    u8g2Fonts.setCursor(todayTableFirstX + (i * forecastW) + 4, todayTableTempsY);
    if(forecasts[i].temp <= friendlyTemp) {
      u8g2Fonts.setForegroundColor(GxEPD_RED);
    } else {
      u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    u8g2Fonts.print(String(forecasts[i].temp, 0) + "°");
    u8g2Fonts.setFont(u8g2_font_open_iconic_weather_4x_t);
    drawWeatherIcon(forecasts[i].icon, todayTableFirstX + 2 + (i * forecastW), todayTableIconsY);
  }
}

//***************************************************************************************************
//  showForecastTempChart
//  
//***************************************************************************************************
void showForecastTempChart() {
  int forecastHour;
  int xOffset;
  int lastSeparator;
  int currentSeparator;
  bool newSection;
  int dow;
  int unixtimeDays;
  
  if (foundForecasts == 0) {
    return;
  }
  float minTemp = 999;
  float maxTemp = -999;

  for (int i = 0; i < foundForecasts; i++) {
    float temp = forecasts[i].temp;
    if (temp > maxTemp) {
      maxTemp = temp;
    }
    if (temp < minTemp) {
      minTemp = temp;
    }
  }
  u8g2Fonts.setFont(u8g2_font_helvR12_tf);
  u8g2Fonts.setCursor(forecastTempX, forecastMaxY);
  if(maxTemp <= friendlyTemp) {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
  } else {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  }
  u8g2Fonts.print(String(maxTemp, 0) + "°");
  u8g2Fonts.setCursor(forecastTempX, forecastMinY);
  if(minTemp <= friendlyTemp) {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
  } else {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  }
  u8g2Fonts.print(String(minTemp, 0) + "°");
  float range = maxTemp - minTemp;
  float barWidth = chartWidth / foundForecasts;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  lastSeparator = chartX;
  // observationTime is in local timetone
  unixtimeDays = floor(conditions.observationTime / 86400);
  dow = (unixtimeDays + 4) % 7;
  for (int i = 0; i < foundForecasts; i++) {
    float temp = forecasts[i].temp;
    float height = (temp - minTemp) * chartHeight / range;
    uint16_t x = chartX + i * barWidth;
    uint16_t y = chartY + chartHeight - height;
    if (i == 0) {
      lastX = x;
      lastY = y;
    }
    display.drawLine(lastX, lastY, x, y, GxEPD_BLACK);
    forecastHour = myTZ.dateTime(forecasts[i].observationTime, "H").toInt();
    // draw days separator
    // forecast is calculated for every 3 hours
    if(forecastHour < 3) {
      // new section -> draw line
      if(forecastHour == 0) xOffset = 0;
      if(forecastHour == 1) xOffset = barWidth / 3;
      if(forecastHour == 2) xOffset = barWidth / 3 * 2;
      currentSeparator = x - xOffset;
      if(currentSeparator > chartX) {
        display.drawLine(currentSeparator, headerH + todayH + forecastHeaderH + 5, 
                         currentSeparator, headerH + todayH + forecastHeaderH + daysH + 5, GxEPD_BLACK);
      }
      newSection = true;
    } else if(i == (MAX_FORECASTS - 1)){
      // last section, don't draw line
      currentSeparator = x;
      newSection = true;
    } else {
      // no new section
      newSection = false;
    }
    if(newSection) {
      if((currentSeparator - lastSeparator) >= minDayW) {
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.setCursor(((x - (currentSeparator - lastSeparator) / 2)) - (minDayW / 2), 
                            headerH + todayH + forecastHeaderH + daysH + 5 - 2);
        u8g2Fonts.print(weekdays[dow]);
      }
      dow = dow + 1;
      if(dow > 6) dow = 0;
      lastSeparator = currentSeparator;
    }
    lastX = x; 
    lastY = y;
  }
}

//***************************************************************************************************
//  drawWeatherIcon
//  
//***************************************************************************************************
void drawWeatherIcon(String icon, int x, int y) {
  // 01d, clear sky
  if (icon == "01d")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_SUNNY);
  }
  // 01n
  if (icon == "01n")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_SUNNY);
  }
  // 02d, few clouds
  if (icon == "02d")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_SUNNY);
  }
  // 02n
  if (icon == "02n")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_SUNNY);
  }
  // 03d, scattered clouds
  if (icon == "03d")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_PARTLY_CLOUDY);
  }
  // 03n
  if (icon == "03n")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_PARTLY_CLOUDY);
  }
  // 04d, broken clouds
  if (icon == "04d")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_CLOUDY);
  }
  // 04n
  if (icon == "04n")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_CLOUDY);
  }
  // 09d, shower rain
  if (icon == "09d")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 09n
  if (icon == "09n")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 10d, rain
  if (icon == "10d")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 10n
  if (icon == "10n")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 11d, thunderstorm
  if (icon == "11d")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 11n
  if (icon == "11n")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_RAIN);
  }
  // 13d, snow
  if (icon == "13d")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_SNOW);
  }
  // 13n
  if (icon == "13n")  {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawGlyph(x, y, ICON_SNOW);
  }
  // 50d, mist
  if (icon == "50d")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_CLOUDY);
  }
  // 50n
  if (icon == "50n")  {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawGlyph(x, y, ICON_CLOUDY);
  }
  // Nothing matched: N/A
}

//***************************************************************************************************
//  showOutdoorSensorData
//  
//***************************************************************************************************
void showOutdoorSensorData() {
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setCursor(sensorLeft, sensorOutsideTop);
  u8g2Fonts.print(String(outdoorSensorTemp, 0) + "°");
  u8g2Fonts.setCursor(sensorRight, sensorOutsideTop);
  u8g2Fonts.print(String(outdoorSensorHumidity, 0) + "%");
  u8g2Fonts.setCursor(sensorRight, sensorOutsideBottom);
  u8g2Fonts.print(String(outdoorSensorPressure, 0) + "hPa");
  u8g2Fonts.setCursor(sensorLeft, sensorOutsideBottom);
  if(outdoorSensorVoltage <= lowVoltage) {
    u8g2Fonts.setForegroundColor(GxEPD_RED);
  } else {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  }
  u8g2Fonts.print(String(outdoorSensorVoltage, 2) + "V");  
}
