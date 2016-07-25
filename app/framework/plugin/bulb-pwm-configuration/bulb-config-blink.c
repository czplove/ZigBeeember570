// *******************************************************************
// * bulb-config-blink.c
// *
// * Code to implement a generic blinking function.  This allows the user to 
// * turn the LED on for a time, off for a time, blink for a time, and blink
// * a specific pattern for a time.  After the blink code terminates, the LED
// * drive will go back to the level specified by the ZigBee attributes of 
// * on/off, level, and color.  
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"

#ifdef EMBER_AF_PLUGIN_SCENES
  #include "app/framework/plugin/scenes/scenes.h"
#endif //EMBER_AF_PLUGIN_SCENES

#ifdef EMBER_AF_PLUGIN_ON_OFF
  #include "app/framework/plugin/on-off/on-off.h"
#endif //EMBER_AF_PLUGIN_ON_OFF

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  #include "app/framework/plugin/zll-level-control-server/zll-level-control-server.h"
#endif //EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER

#include "app/framework/plugin/bulb-pwm-configuration/bulb-config.h"

// **********************************************
// LED Output Blinking State
//
// API for blinking light value for user feedback.

#define pwmBlinkEventControl  emberAfPluginBulbPwmConfigurationBlinkEventFunctionEventControl

EmberEventControl emberAfPluginBulbPwmConfigurationBlinkEventFunctionEventControl;

void emberAfPluginBulbPwmConfigurationBlinkStopCallback( uint8_t endpoint );

enum {
  LED_ON            = 0x00,
  LED_OFF           = 0x01,
  LED_BLINKING_ON   = 0x02,
  LED_BLINKING_OFF  = 0x03,
  LED_BLINK_PATTERN = 0x04,
};

static uint8_t ledEventState = LED_ON;
static uint8_t ledBlinkCount = 0x00;
static uint16_t ledBlinkTime;

#define BLINK_PATTERN_MAX_LENGTH EMBER_AF_PLUGIN_BULB_PWM_CONFIGURATION_BLINK_PATTERN_MAX_LENGTH

// See the function emberAfPluginBulbConfigLedBlinkPattern for an explanation 
// of these variables.
static uint16_t blinkPattern[BLINK_PATTERN_MAX_LENGTH];
static uint8_t blinkPatternLength;
static uint8_t blinkPatternIndex;

static void turnLedOn( void )
{
  emberAfPluginBulbConfigDriveWRGB(emberAfPluginBulbConfigMaxDriveValue(),
                                   emberAfPluginBulbConfigMaxDriveValue(),
                                   emberAfPluginBulbConfigMaxDriveValue(),
                                   emberAfPluginBulbConfigMaxDriveValue());
}

static void turnLedOff( void )
{
  emberAfPluginBulbConfigDriveWRGB(0,0,0,0);
}

static void ledBlinkStop( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;
  
  emberEventControlSetInactive(pwmBlinkEventControl);

  emberAfPluginBulbPwmConfigurationBlinkStopCallback( fixedEndpoints[0] );
}

void emberAfPluginBulbConfigLedOn( uint8_t time )
{
  turnLedOn();
  ledEventState = LED_ON;

  if(time > 0) {
    emberEventControlSetDelayQS(pwmBlinkEventControl,
                                ((uint16_t) time) * 4);
  } else {
    emberEventControlSetInactive(pwmBlinkEventControl);
  }
} 

void emberAfPluginBulbConfigLedOff( uint8_t time )
{
  turnLedOff();
  ledEventState = LED_OFF;

  if(time > 0) {
    emberEventControlSetDelayQS(pwmBlinkEventControl,
                                ((uint16_t) time) * 4);
  } else {
    emberEventControlSetInactive(pwmBlinkEventControl);
  }
}

void emberAfPluginBulbConfigLedBlink( uint8_t count, uint16_t blinkTime )
{
  ledBlinkTime = blinkTime;

  turnLedOff();
  ledEventState = LED_BLINKING_OFF;
  emberEventControlSetDelayMS(pwmBlinkEventControl,
                              ledBlinkTime);
  ledBlinkCount = count;
}

// Implements a function to blink an arbitrary pattern on the bulb output.  The
// function receives a count, which is the number of times to cycle through the
// pattern, a length of the pattern, and a short array of 16 bit integer values
// which are interpreted as the blink pattern in milliseconds.  The first value
// is a number of milliseconds for the light to be on, the second is a number
// of milliseconds for the light to be off, and so on until length has been 
// reached.  For example, if we wished to create an SOS pattern, we would
// program the following array:
// pattern[20] = {500, 100, 500, 100, 500, 100, 100, 100, 100, 100, 100, 100,
//                500, 100, 500, 100, 500, 100};
// Where the light would be on in a sequences of 500 and 100 mS intervals, and 
// the light would be off for 100 mS in between the on intervals.  
void emberAfPluginBulbConfigLedBlinkPattern( uint8_t count, uint8_t length, uint16_t *pattern )
{
  uint8_t i;

  if(length < 2) {
    return;
  }

  turnLedOn();

  ledEventState = LED_BLINK_PATTERN;

  if(length > BLINK_PATTERN_MAX_LENGTH) {
    length = BLINK_PATTERN_MAX_LENGTH ;
  }

  blinkPatternLength = length;
  ledBlinkCount = count;

  for(i=0; i<blinkPatternLength; i++) {
    blinkPattern[i] = pattern[i];
  }

  emberEventControlSetDelayMS(pwmBlinkEventControl,
                              blinkPattern[0]);
  
  blinkPatternIndex = 1;
}


void emberAfPluginBulbPwmConfigurationBlinkEventFunctionEventHandler( void )
{
  switch(ledEventState) {
  case LED_ON:
    ledBlinkStop();
    break;

  case LED_OFF:
    ledBlinkStop();
    break;

  case LED_BLINKING_ON:
    turnLedOff();
    if(ledBlinkCount > 0) {
      if(ledBlinkCount != 255) { // blink forever if count is 255
        ledBlinkCount --;
      }
      if (ledBlinkCount > 0) {
        ledEventState = LED_BLINKING_OFF;
        emberEventControlSetDelayMS(pwmBlinkEventControl,
                                    ledBlinkTime);

      } else {
        ledEventState = LED_OFF;
        ledBlinkStop();
      } 
    } else {
      ledEventState = LED_BLINKING_OFF;
      emberEventControlSetDelayMS(pwmBlinkEventControl,
                                  ledBlinkTime);
    }
    break;
  case LED_BLINKING_OFF:
    turnLedOn();
    ledEventState = LED_BLINKING_ON;
    emberEventControlSetDelayMS(pwmBlinkEventControl,
                                ledBlinkTime);
    break;
  case LED_BLINK_PATTERN:
    if(ledBlinkCount == 0) {
      turnLedOff();

      ledEventState = LED_OFF;
      ledBlinkStop();

      break;
    }

    if(blinkPatternIndex %2 == 1) {
      turnLedOff();
    } else {
      turnLedOn();
    }

    emberEventControlSetDelayMS(pwmBlinkEventControl,
                              blinkPattern[blinkPatternIndex]);
  
    blinkPatternIndex ++;

    if(blinkPatternIndex >= blinkPatternLength) {
      blinkPatternIndex = 0;

      if(ledBlinkCount != 255) { // blink forever if count is 255
        ledBlinkCount --;
      } 
    }

  default:
    break;
  }
}

// blink code CLI
void emAfPluginBulbConfigOnCommand( void )
{
  uint8_t onValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfPluginBulbConfigLedOff( onValue );
}

void emAfPluginBulbConfigOffCommand( void )
{
  uint8_t offValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfPluginBulbConfigLedOff( offValue );
}

void emAfPluginBulbConfigBlinkCommand( void )
{
  uint8_t count = (uint16_t)emberUnsignedCommandArgument(0);
  uint16_t blinkTime = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfPluginBulbConfigLedBlink( count, blinkTime );
}

static uint16_t blinkPattern[] = {
  500,
  100,
  500,
  100,
  500,
  100,
  100,
  100,
  100,
  100,
  100,
  100,
  500,
  100,
  500,
  100,
  500,
  1000
};

void emAfPluginBulbConfigBlinkPatternCommand ( void )
{
  emberAfPluginBulbConfigLedBlinkPattern( 2, 18, blinkPattern );
}

// identify plugin support
void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{
  emberAfPluginBulbConfigLedBlink(0, 1000);
}

void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
  emberAfPluginBulbConfigLedOn(0);
  emberAfPluginBulbPwmConfigurationBlinkStopCallback( endpoint );
}
