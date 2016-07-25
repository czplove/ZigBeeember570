// *******************************************************************
// * bulb-config-dim-indicate.c
// *
// * Code to implement generic dimming indictaiton functinatliy.  While the 
// * bulb-config-blink code will indicate to the user with immediate on/off 
// * functionality, this function implements the indication functionality with 
// * a diming on and dimming off function.
// *
// * Note:  this does not affect the on/off or level control clusters.  
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/framework/plugin/bulb-pwm-configuration/bulb-config.h"

void emberAfPluginBulbPwmConfigurationBlinkStopCallback( uint8_t endpoint );

#define pwmDimIndicateControl  emberAfPluginBulbPwmConfigurationDimEventFunctionEventControl
#define pwmDimIndicateFunction emberAfPluginBulbPwmConfigurationDimEventFunctionEventHandler

EmberEventControl emberAfPluginBulbPwmConfigurationDimEventFunctionEventControl;

#define DIM_UPDATE_TIME_MS 50

enum {
  DIM_DIMMING_DN = 0x01,
  DIM_DIMMING_UP = 0x02,
  DIM_IDLE = 0x00,
};

typedef struct
{
  uint8_t state;
  uint16_t min;
  uint16_t max;
  uint16_t current;
  uint16_t delta;
  uint8_t count;
} PwmIndicateState;

static PwmIndicateState pwmIndicateState;

static void pwmSetValue( uint16_t value ) {
  emberAfPluginBulbConfigDriveWRGB( value, value, value, value );
}

void pwmDimIndicateFunction( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;
  uint16_t newValue;
  uint8_t count;

  switch(pwmIndicateState.state) {
  case DIM_DIMMING_DN:
    newValue = pwmIndicateState.current - pwmIndicateState.delta;
    if(newValue <= pwmIndicateState.min ||
       newValue >= pwmIndicateState.max ) { // did we wrap?
       newValue = pwmIndicateState.min;
       pwmIndicateState.state = DIM_DIMMING_UP;

    }
    break;
  case DIM_DIMMING_UP:
    newValue = pwmIndicateState.current + pwmIndicateState.delta;
    if(newValue >= pwmIndicateState.max) { // did we wrap?
       newValue = pwmIndicateState.max;
       pwmIndicateState.state = DIM_DIMMING_DN;
       
       count = pwmIndicateState.count - 1;

       if(count == 0) {
         pwmIndicateState.state = DIM_IDLE;
       } else {
         pwmIndicateState.count = count;
       }
    }

    break;
  default:
    break;
  }

  pwmIndicateState.current = newValue;
  pwmSetValue( newValue );

  if(pwmIndicateState.state == DIM_IDLE) {
    emberEventControlSetInactive( pwmDimIndicateControl );
    emberAfPluginBulbPwmConfigurationBlinkStopCallback(fixedEndpoints[0]);
  } else {
    pwmIndicateState.current = newValue;
    pwmSetValue(newValue);
    emberEventControlSetDelayMS( pwmDimIndicateControl,
                                 DIM_UPDATE_TIME_MS);
  }
}

void emberAfBulbConfigDimIndicate(uint8_t count, uint16_t timeMs)
{
  uint16_t numberSteps;
  uint16_t stepSize;

  numberSteps = timeMs / DIM_UPDATE_TIME_MS;

  // It is a little wasteful to do this each time, but I wanted to make it
  // clear where the values come from.  
  pwmIndicateState.max = emberAfPluginBulbConfigMaxDriveValue();
  pwmIndicateState.min = emberAfPluginBulbConfigMaxDriveValue();

  stepSize = (pwmIndicateState.max - pwmIndicateState.min) / numberSteps;

  pwmIndicateState.state = DIM_DIMMING_DN;
  pwmIndicateState.current = pwmIndicateState.max;
  pwmIndicateState.delta = stepSize;
  pwmIndicateState.count = count;

  emberEventControlSetActive( pwmDimIndicateControl );
}

void emAfPwmIndicateCommand( void )
{
  uint8_t count = (uint8_t)emberUnsignedCommandArgument(0);
  uint16_t timeMs = (uint16_t)emberUnsignedCommandArgument(1);

  emberAfBulbConfigDimIndicate( count, timeMs );
}

