#include <WiFi.h>
#include "time.h"
#include "sntp.h"
#include <FastLED.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define NTPHOST "pool.ntp.org"
#define AUTODST
#define NODEMCU
#include <NTPClient.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTPHOST, 0, 60000);

#ifdef AUTODST
#include <Timezone.h>                                          // "Timezone" by Jack Christensen
  TimeChangeRule *tcr;
  /* US */
  TimeChangeRule tcr1 = {"tcr1", First, Sun, Nov, 2, -300};   // utc -5h, valid from first sunday of november at 2am
  TimeChangeRule tcr2 = {"tcr2", Second, Sun, Mar, 2, -240};  // utc -4h, valid from second sunday of march at 2am
  Timezone myTimeZone(tcr1, tcr2);
#endif AUTODST


const char* ssid       = "XXXXX";
const char* password   = "XXXX";

const uint8_t digitYPosition = 0;
  #define LED_PWR_LIMIT 500
  #define FRAMES_PER_SECOND  60
  #define LED_PIN 10   
  #define LED_DIGITS 4                                           // 4 or 6 digits, HH:MM or HH:MM:SS (unsupported)
  #define LED_COUNT 90                                           // Total number of leds, 90 on LMGv1 (17x5)
  #if ( LED_DIGITS == 6 )
    #define LED_COUNT 91                                         // leds on the 6 digit version (unsupported)
  #endif
  #if ( LED_DIGITS == 6 )
    #define RES_X 17
  #else
    #define RES_X 17
  #endif
  #define RES_Y 5
  #define CHAR_X 3
  #define CHAR_Y 5

  // start clock specific config/parameters
#if ( LED_DIGITS == 4 )
  const uint8_t digitPositions[4] = { 0, 4, 10, 14 };            // x coordinates of HH:MM
#endif

#if ( LED_DIGITS == 6 )
  const uint8_t digitPositions[6] = { 0, 4, 10, 14, 20, 24 };    // x coordinates of HH:MM:SS
#endif

uint8_t markerHSV[3] = { 0, 127, 20 };                         // this color will be used to "flag" leds for coloring later on while updating the leds
CRGB leds[LED_COUNT];
CRGBPalette16 currentPalette;

uint8_t clockStatus = 1;
time_t localtimenow;

/* Start basic appearance config------------------------------------------------------------------------ */
const bool dotsBlinking = true;                                  // true = only light up dots on even seconds, false = always on
const bool leadingZero = true;                                  // true = enable a leading zero, 9:00 -> 09:00, 1:30 -> 01:30...
uint8_t displayMode = 1;                                         // 0 = 24h mode, 1 = 12h mode ("1" will also override setting that might be written to EEPROM!)
uint8_t colorMode = 1;                                           // different color modes, setting this to anything else than zero will overwrite values written to eeprom, as above
uint16_t colorSpeed = 750;                                       // controls how fast colors change, smaller = faster (interval in ms at which color moves inside colorizeOutput();)
const bool colorPreview = false;                                  // true = preview selected palette/colorMode using "8" on all positions for 3 seconds
const uint8_t colorPreviewDuration = 30;                          // duration in seconds for previewing palettes/colorModes if colorPreview is enabled/true
const bool reverseColorCycling = false;                          // true = reverse color movements
const uint8_t brightnessLevels[3] {96, 150, 240};               // 0 - 255, brightness Levels (min, med, max) - index (0-2) will be saved to eeprom
uint8_t brightness = brightnessLevels[1];                        // default brightness if none saved to eeprom yet / first run
const uint8_t characters[20][CHAR_X * CHAR_Y] PROGMEM = {
  { 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1 },        // 0
  { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 1
  { 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1 },        // 2
  { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1 },        // 3
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1 },        // 4
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // 5
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 6
  { 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 7
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 8
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // 9
  { 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },        // T - some letters from here on (index 10, so won't interfere with digits 0-9)
  { 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0 },        // r
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // y
  { 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // d
  { 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1 },        // C
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0 },        // F
  { 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 },        // m1 - will be drawn 2 times when used with an offset of +2 on x
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },        // °
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1 },        // H
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }         // "blank"
};

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}

void setup()
{
  Serial.begin(115200);
//connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  unsigned long startTimer = millis();
  uint8_t wlStatus = 0;
  uint8_t counter = 6;
  while ( wlStatus == 0 ) {
  if ( WiFi.status() != WL_CONNECTED ) wlStatus = 0; else wlStatus = 1;
    if ( millis() - startTimer >= 1000 ) {
      FastLED.clear();
      showChar(counter, digitPositions[3], digitYPosition);
      FastLED.show();
      if ( counter > 0 ) counter--; else wlStatus = 2;
      startTimer = millis();
    }
  }
  Serial.println(" CONNECTED");

  EEPROM.begin(512);



  // FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalLEDStrip).setTemperature(DirectSunlight).setDither(1);
  // FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, LED_COUNT); 
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalSMD5050).setTemperature(DirectSunlight).setDither(1);
  // FastLED.addLeds<WS2811_400, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_PWR_LIMIT);
  FastLED.clear();
  FastLED.show();
  for ( uint8_t i = 0; i < LED_DIGITS; i++ ) {
  setPixel(0, i);
  }
  FastLED.show();
  

  timeClient.begin();

  FastLED.clear();
  FastLED.show();

  paletteSwitcher();
  brightnessSwitcher();
  colorModeSwitcher();
  displayModeSwitcher();

  timeClient.update();
  time_t timeNTP = timeClient.getEpochTime();
  setTime(timeNTP);

  WiFi.disconnect();

  clockStatus = 0;

}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void demo()
{
  while(1)
  {
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically

  if(Serial.available())
    {
      String command = Serial.readStringUntil('\n');
      if(command == "q")
      {
        break;
      }
    }
  }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, LED_COUNT, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(LED_COUNT) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, LED_COUNT, 10);
  int pos = random16(LED_COUNT);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, LED_COUNT, 20);
  int pos = beatsin16( 13, 0, LED_COUNT-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < LED_COUNT; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, LED_COUNT, 20);
  uint8_t dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, LED_COUNT-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}


void loop()
{
      if(Serial.available())
    {
      String command = Serial.readStringUntil('\n');
      if(command == "p")
      {
        paletteSwitcher();
      }
      if(command == "d")
      {
        displayModeSwitcher();
      }
      if(command == "b")
      {
        brightnessSwitcher();
      }
      if(command == "c")
      {
        colorModeSwitcher();
      }
      if(command == "x")
      {
        demo();
      }
    }


    static uint8_t lastSecondDisplayed = 0;           // This keeps track of the last second when the display was updated (HH:MM and HH:MM:SS)
    static unsigned long lastCheckRTC = millis();     // This will be used to read system time in case no RTC is defined (not supported!)
    static bool doUpdate = false;                     // Update led content whenever something sets this to true. Coloring will always happen at fixed intervals!

    static time_t sysTime = now();                  // if no rtc is defined, get local system time

    static uint8_t refreshDelay = 5;                // refresh leds every 5ms
    static long lastRefresh = millis();             // Keeps track of the last led update/FastLED.show() inside the loop

    if ( millis() - lastCheckRTC >= 50 ) {                                                   // check rtc/system time every 50ms
        sysTime = now();
      if ( lastSecondDisplayed != second(sysTime) ) doUpdate = true;
        lastCheckRTC = millis();
    }

    if ( doUpdate ) {                                      // this will update the led array if doUpdate is true because of a new second from the rtc
    FastLED.clear();                                 // 1A - clear all leds...
    displayTime(now());                              // 2A - output rtcTime to the led array..
    lastSecondDisplayed = second(sysTime);

    syncHelper();

    }

    colorizeOutput(colorMode);                           // 1C, 2C, 3C...colorize the data inside the led array right now...
    if ( millis() - lastRefresh >= refreshDelay ) {
        FastLED.show();
        lastRefresh = millis();
    }

    EVERY_N_SECONDS(10){
      printTime();      
    }
}


void previewMode() {
/*  This will simply display "8" on all positions, speed up the color cyling and preview the
    selected palette or colorMode                                                            */
  // if ( clockStatus == 1 ) return;                           // don't preview when starting up
  unsigned long previewStart = millis();
  uint16_t colorSpeedBak = colorSpeed;
  colorSpeed = 5;
  while ( millis() - previewStart <= uint16_t ( colorPreviewDuration * 1000L ) ) {
    for ( uint8_t i = 0; i < 2; i++ ) {
      showChar(8, digitPositions[i], digitYPosition);
    }
    colorizeOutput(colorMode);
    FastLED.show();
  }
  colorSpeed = colorSpeedBak;
  FastLED.clear();
}

uint16_t calcPixel(uint8_t x, uint8_t y) {                   // returns led # located at x/y
  uint16_t pixel = 0;
  if ( x % 2 == 0 ) {
    pixel = x / 2 + y * ( RES_X + 1 );
  } else {
    pixel = RES_X + 1 - x + x / 2 + y * ( RES_X + 1 );
  }
  return pixel;
}

void colorHelper(uint8_t xpos, uint8_t ypos, uint8_t hue, uint8_t sat, uint8_t bri) {
  for ( uint8_t x = xpos; x < xpos + CHAR_X; x++ ) {
    for ( uint8_t y = ypos; y < ypos + CHAR_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)].setHSV(hue, sat, bri);
    }
  }
}


void displayTime(time_t t) {
  #ifdef AUTODST
    if ( clockStatus < 90 ) {                             // display adjusted times only while NOT in setup
      t = myTimeZone.toLocal(t);                          // convert display time to local time zone according to rules on top of the sketch
    }
  #endif
  if ( clockStatus >= 90 ) {
    FastLED.clear();
  }
  /* hours */
  if ( displayMode == 0 ) {
    if ( hour(t) < 10 ) {
      if ( leadingZero ) {
        showChar(0, digitPositions[0], digitYPosition);
      }
    } else {
      showChar(hour(t) / 10, digitPositions[0], digitYPosition);
    }
    showChar(hour(t) % 10, digitPositions[1], digitYPosition);
  } else if ( displayMode == 1 ) {
    if ( hourFormat12(t) < 10 ) {
      if ( leadingZero ) {
        showChar(0, digitPositions[0], digitYPosition);
      }
    } else {
      showChar(hourFormat12(t) / 10, digitPositions[0], digitYPosition);
    }
    showChar(hourFormat12(t) % 10, digitPositions[1], digitYPosition);
  }
  /* minutes */
  showChar(minute(t) / 10, digitPositions[2], digitYPosition);
  showChar(minute(t) % 10, digitPositions[3], digitYPosition);
  if ( LED_DIGITS == 6 ) {
    /* seconds */
    showChar(second(t) / 10, digitPositions[4], digitYPosition);
    showChar(second(t) % 10, digitPositions[5], digitYPosition);
  }
  if ( clockStatus >= 90 ) {                                      // in setup modes displayTime will also use colorizeOutput/FastLED.show!
    static unsigned long lastRefresh = millis();
    if ( isAM(t) && displayMode == 1 ) {                          // in 12h mode and if it's AM only light up the upper dots (while setting time)
      showDots(1);
    } else {
      showDots(2);
    }
    if ( millis() - lastRefresh >= 25 ) {
      colorizeOutput(colorMode);
      FastLED.show();
      lastRefresh = millis();
    }
    return;
  }
  /* dots */
  if ( dotsBlinking ) {
    if ( second(t) % 2 == 0 ) {
      showDots(2);
    }
  } else {
    showDots(2);
  }
}

void setPixel(uint8_t x, uint8_t y) {
  uint16_t pixel = 0;
  if ( x < RES_X && y < RES_Y ) {
    pixel = calcPixel(x, y);
    leds[pixel].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
  }
}


void showDots(uint8_t dots) {
  // dots 0 = upper dots, dots 1 = lower dots, dots 2 = all dots
  if ( dots == 0  || dots == 2 ) {
    setPixel(digitPositions[1] + CHAR_X + 1, 3);
    if ( LED_DIGITS == 6 ) {
      setPixel(digitPositions[3] + CHAR_X + 1, 3);
    }
  }
  if ( dots == 1 || dots == 2 ) {
    setPixel(digitPositions[1] + CHAR_X + 1, 1);
    if ( LED_DIGITS == 6 ) {
      setPixel(digitPositions[3] + CHAR_X + 1, 1);
    }
  }
}


void showChar(uint8_t character, uint8_t x, uint8_t y) {        // show digit (0-9) with lower left corner at position x/y 
  for ( uint8_t i = 0; i < ( CHAR_X * CHAR_Y ); i++ ) {
    if ((characters[character][i]) == 1 ) {
      setPixel(x + ( i - ( ( i / CHAR_X ) * CHAR_X ) ), ( y + CHAR_Y - 1 ) - ( i / CHAR_X ) );
    }
  }
}


void colorizeOutput(uint8_t mode) {
/* So far showChar()/setPixel() only set some leds inside the array to values from "markerHSV" but we haven't updated
   the leds yet using FastLED.show(). This function does the coloring of the right now single colored but "insivible"
   output. This way color updates/cycles aren't tied to updating display contents                                      */
  static unsigned long lastColorChange = 0;
  static uint8_t startColor = 0;
  static uint8_t colorOffset = 0;              // different offsets result in quite different results, depending on the amount of leds inside each segment...
                                               // ...so it's set inside each color mode if required
  /* mode 0 = simply assign different colors with an offset of "colorOffset" to each led based on its x position -> each digit gets its own color */
  if ( mode == 0 ) {
    colorOffset = 24;
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      for ( uint8_t x = digitPositions[pos]; x < digitPositions[pos] + CHAR_X; x++ ) {
        for ( uint8_t y = 0; y < RES_Y; y++ ) {
          if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - pos * colorOffset, brightness, LINEARBLEND);
        }
      }
    }
    // following will color the dots in the same way
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - 8 * colorOffset, brightness, LINEARBLEND);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)] = ColorFromPalette(currentPalette, startColor - 8 * colorOffset, brightness, LINEARBLEND);
      }
    }
  }
  /* mode 1 = simply assign different colors with an offset of "colorOffset" to each led based on its y coordinate -> each digit with a top/down gradient */
  if ( mode == 1 ) {
    colorOffset = 16;
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      for ( uint8_t x = digitPositions[pos]; x < digitPositions[pos] + CHAR_X; x++ ) {
        for ( uint8_t y = 0; y < RES_Y; y++ ) {
          if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
        }
      }
    }
    // following will color the dots in the same way
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
      }
    }
  }
  /* clockStatus >= 90 is used for coloring output while in setup mode */
  if ( clockStatus >= 90 ) {
    static boolean blinkFlag = true;
    static unsigned long lastBlink = millis();
    static uint8_t b = brightnessLevels[0];
    if ( millis() - lastBlink > 333 ) {                    // blink switch frequency, 3 times a second
      if ( blinkFlag ) {
        blinkFlag = false;
        b = brightnessLevels[1];
      } else {
        blinkFlag = true;
        b = brightnessLevels[0];
      }
      lastBlink = millis();
    }                                                      // unset values = red, set value = green, current value = yellow and blinkinkg
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      if ( clockStatus == 91 ) {  // Y/M/D setup
        colorHelper(digitPositions[0], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[0] + 2, digitYPosition, 0, 255, brightness); // offset for double drawn "M"
        colorHelper(digitPositions[1], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[3], digitYPosition, 64, 255, b);
      }
      if ( clockStatus == 92 ) { // hours
        colorHelper(digitPositions[0], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[1], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[2], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[3], digitYPosition, 0, 255, brightness);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 0, 255, brightness);
          colorHelper(digitPositions[5], digitYPosition, 0, 255, brightness);
        }
      }
      if ( clockStatus == 93 ) {  // minutes
        colorHelper(digitPositions[0], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[1], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[3], digitYPosition, 64, 255, b);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 0, 255, brightness);
          colorHelper(digitPositions[5], digitYPosition, 0, 255, brightness);
        }
      }
      if ( clockStatus == 94 ) {  // seconds
        colorHelper(digitPositions[0], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[1], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[3], digitYPosition, 96, 255, brightness);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 64, 255, b);
          colorHelper(digitPositions[5], digitYPosition, 64, 255, b);
        }
      }
    }
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)].setHSV(24, 255, brightness);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)].setHSV(24, 255, brightness);
      }
    }
  }
}

void paletteSwitcher() {
/* As the name suggests this takes care of switching palettes. When adding palettes, make sure paletteCount increases
  accordingly.  A few examples of gradients/solid colors by using RGB values or HTML Color Codes  below               */
    static uint8_t paletteCount = 6;
    static uint8_t currentIndex = 0;
    if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
        uint8_t tmp = EEPROM.read(0);
        if ( tmp >= 0 && tmp < paletteCount ) {
            currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
        } else {
            currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 6
        }
#ifdef DEBUG
        Serial.print(F("paletteSwitcher(): loaded EEPROM value "));
        Serial.println(tmp);
#endif
    }
    switch ( currentIndex ) {
        case 0: currentPalette = CRGBPalette16( CRGB( 224,   0,  32 ),
                                                CRGB(   0,   0, 244 ),
                                                CRGB( 128,   0, 128 ),
                                                CRGB( 224,   0,  64 ) ); break;
        case 1: currentPalette = CRGBPalette16( CRGB( 224,  16,   0 ),
                                                CRGB( 192,  64,   0 ),
                                                CRGB( 192, 128,   0 ),
                                                CRGB( 240,  40,   0 ) ); break;
        case 2: currentPalette = CRGBPalette16( CRGB::Aquamarine,
                                                CRGB::Turquoise,
                                                CRGB::Blue,
                                                CRGB::DeepSkyBlue   ); break;
        case 3: currentPalette = RainbowColors_p; break;
        case 4: currentPalette = PartyColors_p; break;
        case 5: currentPalette = CRGBPalette16( CRGB::LawnGreen ); break;
    }
#ifdef DEBUG
    Serial.print(F("paletteSwitcher(): selected palette "));
    Serial.println(currentIndex);
#endif
    if ( clockStatus == 0 ) {                             // only save selected palette to eeprom if clock is in normal running mode, not while in startup/setup/whatever
        EEPROM.put(0, currentIndex);
#ifdef NODEMCU
        EEPROM.commit();
#endif
#ifdef DEBUG
        Serial.print(F("paletteSwitcher(): saved index "));
        Serial.print(currentIndex);
        Serial.println(F(" to eeprom"));
#endif
    }
    if ( currentIndex < paletteCount - 1 ) {
        currentIndex++;
    } else {
        currentIndex = 0;
    }
    if ( colorPreview ) {
        previewMode();
    }
#ifdef DEBUG
    Serial.println(F("paletteSwitcher() done"));
#endif
}

  void printTime() {
    /* outputs current system and RTC time to the serial monitor, adds autoDST if defined */
    time_t tmp = now();
        Serial.println(F("-----------------------------------"));
    Serial.print(F("System time is : "));
    if ( hour(tmp) < 10 ) Serial.print(F("0"));
    Serial.print(hour(tmp)); Serial.print(F(":"));
    if ( minute(tmp) < 10 ) Serial.print(F("0"));
    Serial.print(minute(tmp)); Serial.print(F(":"));
    if ( second(tmp) < 10 ) Serial.print(F("0"));
    Serial.println(second(tmp));
    Serial.print(F("System date is : "));
    Serial.print(year(tmp)); Serial.print("-");
    Serial.print(month(tmp)); Serial.print("-");
    Serial.print(day(tmp)); Serial.println(F(" (Y/M/D)"));
        Serial.println(F("-----------------------------------"));
  }


void brightnessSwitcher() {
    static uint8_t currentIndex = 0;
    if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
        uint8_t tmp = EEPROM.read(1);
        if ( tmp >= 0 && tmp < 3 ) {
            currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
        } else {
            currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 2
        }
#ifdef DEBUG
        Serial.print(F("brightnessSwitcher(): loaded EEPROM value "));
        Serial.println(tmp);
#endif
    }
    switch ( currentIndex ) {
        case 0: brightness = brightnessLevels[currentIndex]; break;
        case 1: brightness = brightnessLevels[currentIndex]; break;
        case 2: brightness = brightnessLevels[currentIndex]; break;
    }
#ifdef DEBUG
    Serial.print(F("brightnessSwitcher(): selected brightness index "));
    Serial.println(currentIndex);
#endif
    if ( clockStatus == 0 ) {                             // only save selected brightness to eeprom if clock is in normal running mode, not while in startup/setup/whatever
        EEPROM.put(1, currentIndex);
#ifdef NODEMCU
        EEPROM.commit();
#endif
#ifdef DEBUG
        Serial.print(F("brightnessSwitcher(): saved index "));
        Serial.print(currentIndex);
        Serial.println(F(" to eeprom"));
#endif
    }
    if ( currentIndex < 2 ) {
        currentIndex++;
    } else {
        currentIndex = 0;
    }
#ifdef DEBUG
    Serial.println(F("brightnessSwitcher() done"));
#endif
}

void colorModeSwitcher() {
    static uint8_t currentIndex = 0;
    if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
        if ( colorMode != 0 ) return;                                           // 0 is default, if it's different on startup the config is set differently, so exit here
        uint8_t tmp = EEPROM.read(3);
        if ( tmp >= 0 && tmp < 2 ) {                                            // make sure tmp < 2 is increased if color modes are added in colorizeOutput()!
            currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
        } else {
            currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 2
        }
#ifdef DEBUG
        Serial.print(F("colorModeSwitcher(): loaded EEPROM value "));
        Serial.println(tmp);
#endif
    }
    colorMode = currentIndex;
#ifdef DEBUG
    Serial.print(F("colorModeSwitcher(): selected colorMode "));
    Serial.println(currentIndex);
#endif
    if ( clockStatus == 0 ) {                             // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
        EEPROM.put(3, currentIndex);
#ifdef NODEMCU
        EEPROM.commit();
#endif
#ifdef DEBUG
        Serial.print(F("colorModeSwitcher(): saved index "));
        Serial.print(currentIndex);
        Serial.println(F(" to eeprom"));
#endif
    }
    if ( currentIndex < 1 ) {
        currentIndex++;
    } else {
        currentIndex = 0;
    }
    if ( colorPreview ) {
        previewMode();
    }
#ifdef DEBUG
    Serial.println(F("colorModeSwitcher() done"));
#endif
}


void displayModeSwitcher() {
    static uint8_t currentIndex = 0;
    if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
        if ( displayMode != 0 ) return;                                         // 0 is default, if it's different on startup the config is set differently, so exit here
        uint8_t tmp = EEPROM.read(2);
        if ( tmp >= 0 && tmp < 2 ) {                                            // make sure tmp < 2 is increased if display modes are added
            currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
        } else {
            currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 1 (24h/12h mode)
        }
#ifdef DEBUG
        Serial.print(F("displayModeSwitcher(): loaded EEPROM value "));
        Serial.println(tmp);
#endif
    }
    displayMode = currentIndex;
#ifdef DEBUG
    Serial.print(F("displayModeSwitcher(): selected displayMode "));
    Serial.println(currentIndex);
#endif
    if ( clockStatus == 0 ) {                             // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
        EEPROM.put(2, currentIndex);
#ifdef NODEMCU
        EEPROM.commit();
#endif
#ifdef DEBUG
        Serial.print(F("displayModeSwitcher(): saved index "));
        Serial.print(currentIndex);
        Serial.println(F(" to eeprom"));
#endif
    }
    if ( clockStatus == 0 ) {                             // show 12h/24h for 2 seconds after selected in normal run mode, don't show this on startup (status 1)
        FastLED.clear();
        unsigned long timer = millis();
        while ( millis() - timer <= 2000 ) {
            if ( currentIndex == 0 ) {
                showChar(2, digitPositions[0], digitYPosition);
                showChar(4, digitPositions[1], digitYPosition);
                showChar(18, digitPositions[3], digitYPosition);
            }
            if ( currentIndex == 1 ) {
                showChar(1, digitPositions[0], digitYPosition);
                showChar(2, digitPositions[1], digitYPosition);
                showChar(18, digitPositions[3], digitYPosition);
            }
            colorizeOutput(colorMode);
            if ( millis() % 50 == 0 ) {
                FastLED.show();
            }
#ifdef NODEMCU
            yield();
#endif
        }
    }
    if ( currentIndex < 1 ) {
        currentIndex++;
    } else {
        currentIndex = 0;
    }
#ifdef DEBUG
    Serial.println(F("displayModeSwitcher() done"));
#endif
}

void syncHelper() {
  static unsigned long lastSync = millis();    // keeps track of the last time a sync attempt has been made
  if ( millis() - lastSync < 86400000 && clockStatus != 1 ) return;   // only allow one ntp request once per day
//connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  unsigned long startTimer = millis();
  uint8_t wlStatus = 0;
  uint8_t counter = 6;
  while ( wlStatus == 0 ) {
  if ( WiFi.status() != WL_CONNECTED ) wlStatus = 0; else wlStatus = 1;
    if ( millis() - startTimer >= 1000 ) {
      FastLED.clear();
      showChar(counter, digitPositions[3], digitYPosition);
      FastLED.show();
      if ( counter > 0 ) counter--; else wlStatus = 2;
      startTimer = millis();
    }
  }
  Serial.println(" CONNECTED");
  timeClient.update();
  time_t timeNTP = timeClient.getEpochTime();
  setTime(timeNTP);
  lastSync = millis();
  WiFi.disconnect();
}



