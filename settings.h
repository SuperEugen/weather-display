//***************************************************************************************************
//  settings:     Adapt to your liking.
//***************************************************************************************************

// see texts.h for available options
#define LANG                        'DE'          // debug in english, display defined here

// timezone
const char* timezone =              "Europe/Berlin";

// wifi settings
const char *ssid =                  SECRET_WIFI_SSID;
const char *wifiPassword =          SECRET_WIFI_PASSWORD;

// mqtt settings
const char* mqttBroker =            SECRET_MQTT_BROKER;
const int mqttPort =                SECRET_MQTT_PORT;
const char* mqttUser =              SECRET_MQTT_USER;
const char* mqttPassword =          SECRET_MQTT_PASSWORD;

// MQTT settings
const char* mqttClientID =          "S-I-WeatherDisplay";

// mqtt topics
const char* mqttTopicTemp =         "weather-sensor/temp";
const char* mqttTopicHumidity =     "weather-sensor/humidity";
const char* mqttTopicPressure =     "weather-sensor/pressure";
const char* mqttTopicVoltage =      "weather-sensor/voltage";

// Open Weather Map settings
const int updateIntervall =         20;           // Update every 20 minutes
String owmLocationID =              "2924770";    // Berlin Friedenau
String owmLanguage =                "de";
const bool owmIsMetric =            true;
const bool owmIs12HrStyle =         false;

// friendly temperature
const int friendlyTemp =            0;

// low outdoor sensor battery level
const float lowVoltage =            3.10;

// screen layout
const int headerH =                 40;           // followed by 3 lines
const int todayH =                  130;          // followed by 2 lines
const int todayHeaderH =            40;
const int tableSeparation =         15;
const int forecastHeaderH =         20;
const int daysH = 18;

const int nowW =                    110;          // followed by 5 x 1 line
const int forecastW =               57;           // should be (400 - nowW - 5) / 5
const int tempW =                   nowW;
const int dayW =                    96;           // should be (400 - tempW - 3) / 3
const int minDayW =                 30;

const int chartHeight =             80;
const int chartWidth =              296;
const int chartX =                  nowW + 4;
const int chartY =                  218;

const int cityX =                   10;
const int cityY =                   26;
const int outsideX =                200;
const int outsideY =                26;
const int sensorLeft =              270;
const int sensorRight =             330;
const int sensorOutsideTop =        16;
const int sensorOutsideBottom =     36;
const int todayX =                  cityX;
const int todayY =                  70;
const int updatedX =                outsideX;
const int updatedY =                60;
const int updatedTimeX =            sensorLeft;
const int updatedTimeY =            updatedY;
const int sunX =                    updatedX;
const int sunY =                    80;
const int daylightX =               updatedTimeX;
const int daylightY =               sunY;
const int conditionX =              cityX;
const int conditionY =              90;
const int todayNowX =               36;
const int todayNowIconY =           145;
const int todayNowTempY =           162;
const int todayTableFirstX =        nowW + 16;
const int todayTableHoursY =        110;
const int todayTableIconsY =        148;
const int todayTableTempsY =        170;
const int forecastX =               cityX;
const int forecastY =               200;
const int forecastTempX =           nowW - 25;
const int forecastMaxY =            234;
const int forecastMinY =            296;
const int forecastTableFirstX =     120;
const int forecastTableDaysY =      266;
