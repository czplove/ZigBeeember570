// *****************************************************************************
// * led-blink.h
// *
// * API to allow for basic control of activity LEDs, including on/off
// * functionality and blink pattern creation.
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/event.h"
#include "hal/hal.h"
#include "hal/micro/micro.h"
#include "hal/micro/led-blink.h"

//------------------------------------------------------------------------------
// Private plugin macros

// Register macros helpful for GPIO interaction
#define GPIO_PxCLR_BASE (GPIO_PACLR_ADDR)
#define GPIO_PxSET_BASE (GPIO_PASET_ADDR)
#define GPIO_PxOUT_BASE (GPIO_PAOUT_ADDR)
// Each port is offset from the previous port by the same amount
#define GPIO_Px_OFFSET  (GPIO_PBCFGL_ADDR-GPIO_PACFGL_ADDR)

// Length restriction for LED pattern
#define MAX_BLINK_PATTERN_LENGTH  20


//------------------------------------------------------------------------------
// Plugin events
EmberEventControl emberAfPluginLedBlinkLedEventFunctionEventControl;

//------------------------------------------------------------------------------
// Plugin private types and enums
enum {
  LED_ON            = 0x00,
  LED_OFF           = 0x01,
  LED_BLINKING_ON   = 0x02,
  LED_BLINKING_OFF  = 0x03,
  LED_BLINK_PATTERN = 0x04,
};

typedef uint8_t gpioBlinkState;

//------------------------------------------------------------------------------
// Forward declaration of private plugin functions
static void turnLedOn( uint8_t led );
static void turnLedOff( uint8_t led );
static void setBit(uint8_t *data, uint8_t bit);
static void clearBit(uint8_t *data, uint8_t bit);

//------------------------------------------------------------------------------
// Plugin private global variables
static gpioBlinkState ledEventState = LED_ON;
static uint8_t ledBlinkCount = 0x00;
static uint16_t ledBlinkTime;
static uint8_t activeLed = BOARDLED1;
static uint16_t blinkPattern[MAX_BLINK_PATTERN_LENGTH];
static uint8_t blinkPatternLength;
static uint8_t blinkPatternPointer;

//------------------------------------------------------------------------------
// Plugin registered callback implementations

//------------------------------------------------------------------------------
// Plugin event handlers
void emberAfPluginLedBlinkLedEventFunctionEventHandler( void )
{
  switch(ledEventState) {
  case LED_ON:
    // was on.  this must be time to turn it off.
    turnLedOff(activeLed);
    emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);
    break;

  case LED_OFF:
    // was on.  this must be time to turn it off.
    turnLedOn(activeLed);
    emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);
    break;

  case LED_BLINKING_ON:
    turnLedOff(activeLed);
    if (ledBlinkCount > 0) {
      if (ledBlinkCount != 255) { // blink forever if count is 255
        ledBlinkCount --;
      }
      if (ledBlinkCount > 0) {
        ledEventState = LED_BLINKING_OFF;
        emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                    ledBlinkTime);

      } else {
        ledEventState = LED_OFF;
        emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);
      }
    } else {
      ledEventState = LED_BLINKING_OFF;
      emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                  ledBlinkTime);
    }
    break;
  case LED_BLINKING_OFF:
    turnLedOn(activeLed);
    ledEventState = LED_BLINKING_ON;
    emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                ledBlinkTime);
    break;
  case LED_BLINK_PATTERN:
    if (ledBlinkCount == 0) {
      turnLedOff(activeLed);

      ledEventState = LED_OFF;
      emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);

      break;
    }

    if (blinkPatternPointer %2 == 1) {
      turnLedOff(activeLed);
    } else {
      turnLedOn(activeLed);
    }

    emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                blinkPattern[blinkPatternPointer]);

    blinkPatternPointer ++;

    if (blinkPatternPointer >= blinkPatternLength) {
      blinkPatternPointer = 0;
      if (ledBlinkCount != 255) { // blink forever if count is 255
        ledBlinkCount --;
      }
    }

  default:
    break;
  }
}

//------------------------------------------------------------------------------
// Plugin public API function implementations

void halLedBlinkLedOn( uint8_t time )
{
  turnLedOn(activeLed);
  ledEventState = LED_ON;

  if (time > 0) {
    emberEventControlSetDelayQS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                ((uint16_t) time) * 4);
  } else {
    emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);
  }
}

void halLedBlinkLedOff( uint8_t time )
{
  turnLedOff(activeLed);
  ledEventState = LED_OFF;

  if (time > 0) {
    emberEventControlSetDelayQS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                                ((uint16_t) time) * 4);
  } else {
    emberEventControlSetInactive(emberAfPluginLedBlinkLedEventFunctionEventControl);
  }
}

void halLedBlinkBlink( uint8_t count, uint16_t blinkTime)
{
  ledBlinkTime = blinkTime;
  turnLedOff(activeLed);
  ledEventState = LED_BLINKING_OFF;
  emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                              ledBlinkTime);
  ledBlinkCount = count;
}

void halLedBlinkPattern( uint8_t count, uint8_t length, uint16_t *pattern)
{
  uint8_t i;

  if (length < 2) {
    return;
  }

  turnLedOn(activeLed);

  ledEventState = LED_BLINK_PATTERN;

  if (length > MAX_BLINK_PATTERN_LENGTH) {
    length = MAX_BLINK_PATTERN_LENGTH;
  }

  blinkPatternLength = length;
  ledBlinkCount = count;

  for(i=0; i<blinkPatternLength; i++) {
    blinkPattern[i] = pattern[i];
  }

  emberEventControlSetDelayMS(emberAfPluginLedBlinkLedEventFunctionEventControl,
                              blinkPattern[0]);

  blinkPatternPointer = 1;
}

void halLedBlinkSetActivityLed(uint8_t led)
{
  activeLed = led;
}

//------------------------------------------------------------------------------
// Plugin private function implementations

// *****************************************************************************
// function to set the GPIO and maintain the state during sleep.
// Port is 0 for port a, 1 for port b, and 2 for port c.
void halLedBlinkSleepySetGpio( uint8_t port, uint8_t pin )
{
  assert( port < 3 );
  assert( pin < 8 );

  *((volatile uint32_t *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(port)))) = BIT(pin);

  // Make sure this stays set during the next power cycle
  setBit(&(gpioOutPowerUp[port]), pin);
  setBit(&(gpioOutPowerDown[port]), pin);
}

// *****************************************************************************
// function to clear the GPIO and maintain the state during sleep.
// Port is 0 for port a, 1 for port b, and 2 for port c.
void halLedBlinkSleepyClearGpio( uint8_t port, uint8_t pin )
{
  assert( port < 3 );
  assert( pin < 8 );

  *((volatile uint32_t *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(port)))) = BIT(pin);

  // Make sure this stays clear during the next power cycle
  clearBit(&(gpioOutPowerUp[port]), pin);
  clearBit(&(gpioOutPowerDown[port]), pin);
}

// *****************************************************************************
// Helper function to set a bit
static void setBit(uint8_t *data, uint8_t bit)
{
  uint8_t mask = 0x01 << bit;

  *data = *data | mask;
}

// *****************************************************************************
// Helper function to clear a bit
static void clearBit(uint8_t *data, uint8_t bit)
{
  uint8_t mask = ~(0x01 << bit);

  *data = *data & mask;
}

// *****************************************************************************
// Drive the LED for a GPIO high and update sleepy state
static void turnLedOn( uint8_t led )
{
  uint8_t port = (led) >> 3;
  uint8_t pin = (led) & 0x07;

  halLedBlinkSleepyClearGpio( port, pin );
}

// *****************************************************************************
// Drive the LED for a GPIO low and update sleepy state
static void turnLedOff( uint8_t led )
{

  uint8_t port = (led) >> 3;
  uint8_t pin = (led) & 0x07;

  halLedBlinkSleepySetGpio( port, pin );
}
