// Anaire Minimal CO2, temperature and humidity measurement device www.anaire.org
// Based on https://developer.sensirion.com/tutorials/create-your-own-co2-monitor/
// TTGO T-Display board and Sensirion SCD30 CO2, temp and humidity sensor
// Top button click: toggles buzzer sound
// Bottom button click: sleep; click again the button to wake up

// TTGO ESP32 board
#include "esp_timer.h"
#include <Wire.h>

// Display
// Go to TTGO T-Display's Github Repository
// Download the code as zip, extract it and copy the Folder TFT_eSPI
//  => https://github.com/Xinyuan-LilyGO/TTGO-T-Display/archive/master.zip
// to your Arduino library path
#include <TFT_eSPI.h>
#include <SPI.h>
#define GFXFF 1
#define FF99  &SensirionSimple25pt7b
#define FF90  &ArchivoNarrow_Regular10pt7b
#define FF95  &ArchivoNarrow_Regular50pt7b
#include "anaire_ttgo_splash.h" // Anaire splash screen
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke library, pins defined in User_Setup.h

// Buttons: Top and bottom considered when USB connector is positioned on the right of the board
#include "Button2.h"
#define BUTTON_TOP 35
#define BUTTON_BOTTOM  0
Button2 button_top(BUTTON_TOP);
Button2 button_bottom(BUTTON_BOTTOM);

// Sensirion SCD30 CO2, temperature and humidity sensor
// Download the SeeedStudio SCD30 Arduino driver here:
//  => https://github.com/Seeed-Studio/Seeed_SCD30/releases/latest
#include "SCD30.h"
#define SCD30_SDA_pin 26  // Define the SDA pin used for the SCD30
#define SCD30_SCL_pin 27  // Define the SCL pin used for the SCD30
#include "Sensirion_GadgetBle_Lib.h"  // to connect to Sensirion MyAmbience Android App available on Google Play
GadgetBle gadgetBle = GadgetBle(GadgetBle::DataType::T_RH_CO2_ALT);

// AZ-Delivery Active Buzzer
#define BUZZER_GPIO 12 // signal GPIO12 (pin TOUCH5/ADC15/GPIO12 on TTGO)
bool sound = true;

// control loop timing
static int64_t lastMmntTime = 0;
static int startCheckingAfterUs = 5000000; // 5s

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void button_init()
{
    // Top button click: toggles sound
    button_top.setClickHandler([](Button2 & b) {
        Serial.println("Top button click: toggle sound");
        if (sound) {sound = false;}
        else {sound = true;}
    });
    
    // Bottom button click: sleep
    button_bottom.setClickHandler([](Button2 & b) {
        Serial.println("=> Bottom button click: sleep");
        int r = digitalRead(TFT_BL);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Press bottom button to wake up",  tft.width() / 2, tft.height() / 2 );
        espDelay(6000);
        digitalWrite(TFT_BL, !r);

        tft.writecommand(TFT_DISPOFF);
        tft.writecommand(TFT_SLPIN);
        //After using light sleep, you need to disable timer wake, because here use external IO port to wake up
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        // esp_sleep_enable_ext1_wakeup(GPIO_SEL_0, ESP_EXT1_WAKEUP_ALL_LOW);
        // set bottom button for wake up
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
        delay(200);
        esp_deep_sleep_start();
    });
    
}

void button_loop()
{
    button_top.loop();
    button_bottom.loop();
}

void displayInit() {
  tft.init();
  tft.setRotation(1);
}

void displaySplashScreen() {
   tft.pushImage(0, 0,  240, 135, anaire_ttgo_splash);
}

void displayCo2(uint16_t co2, float temp, float hum) {

  if (co2 > 9999) {
    co2 = 9999;
  }

  uint8_t defaultDatum = tft.getTextDatum();

  // Set screen and text colours based on CO2 value
  if (co2 >= 1000 ) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    if (sound) {digitalWrite(BUZZER_GPIO, HIGH);}
    delay(1000);
    digitalWrite(BUZZER_GPIO, LOW);
  } else if (co2 >= 700 ) {
    tft.fillScreen(TFT_YELLOW);
    tft.setTextColor(TFT_RED, TFT_YELLOW);
    if (sound) {digitalWrite(BUZZER_GPIO, HIGH);}
    delay(100);
    digitalWrite(BUZZER_GPIO, LOW);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    digitalWrite(BUZZER_GPIO, LOW);
  }

  // Draw CO2 number
  tft.setTextDatum(8); // bottom right
  tft.setTextSize(1);
  tft.setFreeFont(FF95);
  tft.drawString(String(co2), 195, 105);

  // Draw CO2 units
  tft.setTextSize(1);
  tft.setFreeFont(FF90);
  tft.drawString("ppm", 230, 90);

  tft.setTextSize(1);
  tft.setFreeFont(FF90);

  // Draw temperature
  tft.setTextDatum(6); // bottom left
  tft.drawString(String(temp), 10, 125);
  tft.drawString("C", 50, 125);

  // Draw humidity
  tft.drawString(String(hum), 90, 125);
  tft.drawString("%", 130, 125);

  // Draw bluetooth device id
  tft.setTextDatum(8); // bottom right
  tft.drawString(gadgetBle.getDeviceIdString(), 230, 125);
  
  // Revert datum setting
  tft.setTextDatum(defaultDatum);
  
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize the GadgetBle Library
  gadgetBle.begin();
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(gadgetBle.getDeviceIdString());

  // Initialize the SCD30 driver
  Wire.begin(SCD30_SDA_pin, SCD30_SCL_pin);
  scd30.initialize();
  scd30.setAutoSelfCalibration(true);

  // Initialize BUZZER to OFF
  pinMode(BUZZER_GPIO, OUTPUT);
  digitalWrite(BUZZER_GPIO, LOW);

  // Initialize buttons
  button_init();
  
  // Display init and splash screen
  displayInit();
  displaySplashScreen();
  delay(3000); // Enjoy the splash screen for 3 seconds
  
}

void loop() {
  
  float result[3] = {0};

  if (esp_timer_get_time() - lastMmntTime >= startCheckingAfterUs) {

    if (scd30.isAvailable()) {   

      // Read SCD30
      scd30.getCarbonDioxideConcentration(result);

      // Write measurements to bluetooth
      gadgetBle.writeCO2(result[0]);
      gadgetBle.writeTemperature(result[1]);
      gadgetBle.writeHumidity(result[2]);
      gadgetBle.commit();
  
      // Provide the sensor values for Tools -> Serial Monitor or Serial Plotter
      Serial.print("CO2[ppm]:");
      Serial.print(result[0]);
      Serial.print("\t");
      Serial.print("Temperature[℃]:");
      Serial.print(result[1]);
      Serial.print("\t");
      Serial.print("Humidity[%]:");
      Serial.println(result[2]);
  
      // display CO2, temperature and humidity values on the display
      displayCo2((uint16_t) round(result[0]), result[1], result[2]);
      
    }
    
    else {
      result[0] = 0;
      result[1] = 0;
      result[2] = 0;
      Serial.println("Error SCD30");
    }
    
    lastMmntTime = esp_timer_get_time();
  
  }
    
  gadgetBle.handleEvents();
  button_loop();
  
}