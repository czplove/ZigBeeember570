// *******************************************************************
// * led-temp-pwm.c
// *
// * Implements the color control server for color temperature bulbs.  Note: 
// * this is HA certifable and has passed HA certification for one customer
// * project.  
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"

#include "hal/micro/cortexm3/diagnostic.h"
#include "app/framework/plugin/bulb-pwm-configuration/bulb-config.h"

#ifdef EMBER_AF_PLUGIN_SCENES
  #include "app/framework/plugin/scenes/scenes.h"
#endif //EMBER_AF_PLUGIN_SCENES

#ifdef EMBER_AF_PLUGIN_ON_OFF
  #include "app/framework/plugin/on-off/on-off.h"
#endif //EMBER_AF_PLUGIN_ON_OFF

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  #include "app/framework/plugin/zll-level-control-server/zll-level-control-server.h"
#endif //EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER

enum {
  EMBER_ZCL_COLOR_TEMP_MOVE_MODE_STOP = 0x00,
  EMBER_ZCL_COLOR_TEMP_MOVE_MODE_UP   = 0x01,
  EMBER_ZCL_COLOR_TEMP_MOVE_MODE_DOWN = 0x03,
}; 

// ----- declarations for the color transition events --------
#define colorTempTransitionEventControl  emberAfPluginLedTempPwmTransitionEventControl

EmberEventControl colorTempTransitionEventControl;

#define UPDATE_TIME_MS 100
typedef struct {
  uint16_t initialTemp;
  uint16_t currentTemp;
  uint16_t finalTemp;
  uint16_t stepsRemaining;
  uint16_t stepsTotal;
  uint16_t endpoint;
} ColorTransitionState;

static ColorTransitionState colorTransitionState;

// ---------- Hardware values required for computing drive levels ----------
static uint16_t minColor, maxColor;
static uint16_t minPwmDrive, maxPwmDrive;

#define MIN_COLOR_DEFAULT 155
#define MAX_COLOR_DEFAULT 360

static uint8_t currentEndpoint( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;

  return(fixedEndpoints[0]);
}

static void driveWRGB( uint16_t white, uint16_t red, uint16_t green, uint16_t blue )
{
  emberAfPluginBulbConfigDriveWRGB(white, red, green, blue);
}


static void updateDriveLevel( uint8_t endpoint);
void emberAfPluginLedTempPwmInitCallback( void )
{
  uint8_t endpoint = currentEndpoint();
  EmberAfStatus status;

  minPwmDrive = emberAfPluginBulbConfigMinDriveValue();
  maxPwmDrive = emberAfPluginBulbConfigMaxDriveValue();

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_COLOR_CONTROL_CLUSTER_ID,
                                      ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_ATTRIBUTE_ID,
                                      (uint8_t *)&minColor,
                                      sizeof(minColor));

  if(status != EMBER_ZCL_STATUS_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Color Temp:  no color temp physical min attribute.\r\n");
    minColor = MIN_COLOR_DEFAULT;
  }

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_COLOR_CONTROL_CLUSTER_ID,
                                      ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_ATTRIBUTE_ID,
                                      (uint8_t *)&maxColor,
                                      sizeof(maxColor));

  if(status != EMBER_ZCL_STATUS_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Color Temp:  no color temp physical max attribute.\r\n");
    maxColor = MAX_COLOR_DEFAULT;
  }
  
  emberSerialPrintf(APP_SERIAL, "Color Temp Init %d %d %d %d\r\n", minPwmDrive, maxPwmDrive, minColor, maxColor);

  updateDriveLevel(endpoint);
}

static void computeRgbFromColorTemp( uint8_t endpoint )
{
  uint16_t currentTemp;
  uint8_t onOff, currentLevel;

  //emberSerialPrintf(APP_SERIAL, "COMPUTE RGB\r\n");
  uint32_t R32, W32;
  uint16_t rDrive, wDrive;

  // during framework init, this funciton sometimes is called before we set up
  // the values for max/min color temperautre.  
  if(maxColor == 0 ||
    minColor == 0)
    return;

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                             (uint8_t *)&currentTemp,
                             sizeof(currentTemp));
  
  emberAfReadServerAttribute(endpoint,
                             ZCL_LEVEL_CONTROL_CLUSTER_ID,
                             ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                             (uint8_t *)&currentLevel,
                             sizeof(currentLevel));

  emberAfReadServerAttribute(endpoint,
                             ZCL_ON_OFF_CLUSTER_ID,
                             ZCL_ON_OFF_ATTRIBUTE_ID,
                             (uint8_t *)&onOff,
                             sizeof(onOff));

  if(onOff == 0 || currentLevel == 0) {
    driveWRGB(0,0,0,0);
    
    return;
  }

  //bounds checking of the attribute temp. 
  if(currentTemp > maxColor)
    currentTemp = maxColor;
  else if(currentTemp < minColor)
    currentTemp = minColor;
     
  // First, compute the R and W transfer
  W32 = maxPwmDrive - (minPwmDrive *6);
  W32 *= (currentTemp - minColor);
  W32 /= (maxColor - minColor);
  W32 += (minPwmDrive *6);

  R32 = maxPwmDrive - (minPwmDrive *6);
  R32 *= (maxColor - currentTemp);
  R32 /= (maxColor - minColor);
  R32 += (minPwmDrive *6);

  // Handle level
  R32 *= (currentLevel - 1);
  R32 /= 253;
  if(R32 < minPwmDrive)
    R32 = 0;

  W32 *= (currentLevel - 1);
  W32 /= 253;
  if(W32 < minPwmDrive)
    W32 = 0;

  // convert to uint16_t and drive the PWMs.
  rDrive = (uint16_t) R32;
  wDrive = (uint16_t) W32;
  
  //emberAfAppPrintln("Setting level now: %d", rDrive);

  driveWRGB(wDrive, rDrive, 0,0);

}

/** @brief Move To Color
 *
 * 
 *
 * @param colorX   Ver.: always
 * @param colorY   Ver.: always
 * @param transitionTime   Ver.: always
 */
bool emberAfColorControlClusterMoveToColorCallback(uint16_t colorX,
                                                      uint16_t colorY,
                                                      uint16_t transitionTime)
{
  return true;
}

/** @brief Server Attribute Changedyes.
 *
 * Level Control cluster, Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfLevelControlClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                              EmberAfAttributeId attributeId)
{
  computeRgbFromColorTemp( endpoint );
}

void emberAfOnOffClusterServerAttributeChangedCallback(uint8_t endpoint, 
                                                       EmberAfAttributeId attributeId)
{
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, attributeId );
}

/** @brief Color Control Cluster Server Attribute Changed
 *
 * Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfColorControlClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                              EmberAfAttributeId attributeId)
{
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, attributeId );
}

/** @brief Move To Color Temperature
 *
 * 
 *
 * @param colorTemperature   Ver.: always
 * @param transitionTime   Ver.: always
 */

static void kickOffTemperatureTransition( uint16_t currentTemp, 
                                          uint16_t newTemp, 
                                          uint16_t transitionTime,
                                          uint8_t endpoint);

bool emberAfColorControlClusterMoveToColorTemperatureCallback(uint16_t colorTemperature,
                                                                 uint16_t transitionTime)
{
  uint8_t endpoint = emberAfCurrentEndpoint();
  uint16_t currentTemp;

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                             (uint8_t *)&currentTemp,
                             sizeof(currentTemp));

  kickOffTemperatureTransition( currentTemp, 
                                colorTemperature, 
                                transitionTime, 
                                endpoint);
 
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);

  return true;
}

/** @brief Move To Hue And Saturation
 *
 * 
 *
 * @param hue   Ver.: always
 * @param saturation   Ver.: always
 * @param transitionTime   Ver.: always
 */
bool emberAfColorControlClusterMoveToHueAndSaturationCallback(uint8_t hue,
                                                                 uint8_t saturation,
                                                                 uint16_t transitionTime)
{
  return true;
}

static void updateDriveLevel( uint8_t endpoint )
{
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, 0x0000 );
}

void emberAfPluginLedTempPwmTransitionEventHandler( void )
{
  uint16_t currentTemp;
  ColorTransitionState *p = &colorTransitionState;
  uint32_t currentTemp32u;

  (p->stepsRemaining)--;

  if(p->finalTemp == p->currentTemp) {
    currentTemp = p->currentTemp;
  } else if(p->finalTemp > p->currentTemp) {
    currentTemp32u  = ((uint32_t) (p->finalTemp - p->initialTemp));
    currentTemp32u *= ((uint32_t) (p->stepsRemaining));
    currentTemp32u /= ((uint32_t) (p->stepsTotal));
    currentTemp = p->finalTemp - ((uint16_t) (currentTemp32u));
  } else {
    currentTemp32u  = ((uint32_t) (p->initialTemp - p->finalTemp));
    currentTemp32u *= ((uint32_t) (p->stepsRemaining));
    currentTemp32u /= ((uint32_t) (p->stepsTotal));
    currentTemp = p->finalTemp + ((uint16_t) (currentTemp32u));
  }

  // need to write colorX and colorY here.  Note:  I am ignoring transition 
  // time for now.  
  emberAfWriteAttribute(colorTransitionState.endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&currentTemp,
                        ZCL_INT16U_ATTRIBUTE_TYPE);

  //computeRgbFromColorTemp(colorTransitionState.endpoint);
  
  p->currentTemp = currentTemp;

  // Write steps remaining here.  Note:  I am asuming 1/10 of a second per
  // step.
  emberAfWriteAttribute(colorTransitionState.endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&(p->stepsRemaining),
                        ZCL_INT16U_ATTRIBUTE_TYPE);


  if(p->stepsRemaining > 0) {
    emberEventControlSetDelayMS( colorTempTransitionEventControl, 
                                 UPDATE_TIME_MS );
  } else {
    emberEventControlSetInactive( colorTempTransitionEventControl );
  }
}

static void stopColorTempTransition( void )
{
  uint16_t stepsRemaining = 0;
  
  emberAfWriteAttribute(colorTransitionState.endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&stepsRemaining,
                        ZCL_INT16U_ATTRIBUTE_TYPE);

  emberEventControlSetInactive( colorTempTransitionEventControl );
}

static void kickOffTemperatureTransition( uint16_t currentTemp, 
                                          uint16_t newTemp, 
                                          uint16_t transitionTime,
                                          uint8_t endpoint) {
  uint32_t stepsRemaining32u;
  uint16_t stepsRemaining, frameworkMin, frameworkMax;

  // first, make sure we are transitioning into a valid temperature range.
  emberAfReadServerAttribute(currentEndpoint(),
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_ATTRIBUTE_ID,
                             (uint8_t *) &frameworkMin,
                             sizeof(frameworkMin));

  emberAfReadServerAttribute(currentEndpoint(),
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_ATTRIBUTE_ID,
                             (uint8_t *) &frameworkMax,
                             sizeof(frameworkMax));

  if(newTemp > frameworkMax) 
    newTemp = frameworkMax;
  if(newTemp < frameworkMin)
    newTemp = frameworkMin;

  emberSerialPrintf(APP_SERIAL, "kick:  %d %d %d %x\r\n", currentTemp, newTemp, transitionTime, endpoint);

  stepsRemaining32u = (uint32_t) transitionTime;
  stepsRemaining32u *= 100; // convert to mS
  stepsRemaining32u /= (uint32_t) UPDATE_TIME_MS;
  stepsRemaining32u++;

  if(stepsRemaining32u > 0xffff)
    stepsRemaining32u--;

  stepsRemaining = (uint16_t) stepsRemaining32u;
  // making an immediate jump, so there is N+1 steps.

  colorTransitionState.initialTemp = currentTemp;
  colorTransitionState.currentTemp = currentTemp;
  colorTransitionState.finalTemp = newTemp;
  colorTransitionState.stepsRemaining = stepsRemaining;
  colorTransitionState.stepsTotal = stepsRemaining;
  colorTransitionState.endpoint = endpoint;

  emberEventControlSetActive( colorTempTransitionEventControl );
}

// new functions for HA compliance
// Note:  need to clean things up a bit.
static uint16_t checkMinMaxValues( uint16_t *commandMin, uint16_t *commandMax)
{
  uint16_t frameworkMin, frameworkMax, currentTemp;

  emberAfReadServerAttribute(currentEndpoint(),
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_ATTRIBUTE_ID,
                             (uint8_t *) &frameworkMin,
                             sizeof(frameworkMin));

  emberAfReadServerAttribute(currentEndpoint(),
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_ATTRIBUTE_ID,
                             (uint8_t *) &frameworkMax,
                             sizeof(frameworkMax));

  emberAfReadServerAttribute(currentEndpoint(),
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                             (uint8_t *)&currentTemp,
                             sizeof(currentTemp));

  // first, check to see if either is outside bounds
  if(*commandMin < frameworkMin) {
    *commandMin = frameworkMin;
  }

  if(*commandMax > frameworkMax ||
     *commandMax == 0) {
    *commandMax = frameworkMax;
  }

  // next, check to make sure the max is above the current temp, and min is
  // below the current temp.
  if(*commandMin > currentTemp) {
    *commandMin = currentTemp;
  }

  if(*commandMax < currentTemp) {
    *commandMax = currentTemp;
  }

  return currentTemp;
  
}

bool emberAfColorControlClusterMoveColorTemperatureCallback(uint8_t moveMode,
                                                               uint16_t rate,
                                                               uint16_t colorTemperatureMinimum,
                                                               uint16_t colorTemperatureMaximum)
{
  uint16_t currentTemp =
    checkMinMaxValues( &colorTemperatureMinimum, &colorTemperatureMaximum);
  uint16_t transitTime;

  switch(moveMode) {
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_STOP:
    stopColorTempTransition();
    break;
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_UP:
    transitTime = colorTemperatureMaximum - currentTemp;
    transitTime *= (1000 / UPDATE_TIME_MS);
    transitTime /= rate;

    kickOffTemperatureTransition( currentTemp,
                                  colorTemperatureMaximum,
                                  transitTime,
                                  currentEndpoint() );
    break;
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_DOWN:
    transitTime = currentTemp - colorTemperatureMinimum;
    transitTime *= (1000 / UPDATE_TIME_MS);
    transitTime /= rate;

    kickOffTemperatureTransition( currentTemp,
                                  colorTemperatureMinimum,
                                  transitTime,
                                  currentEndpoint() );
    break;
  } 

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);

  return true;
}

bool emberAfColorControlClusterStepColorTemperatueCallback(uint8_t stepMode,
                                                              uint16_t stepSize,
                                                              uint16_t transitionTime,
                                                              uint16_t colorTemperatureMinimum,
                                                              uint16_t colorTemperatureMaximum)
{
  uint16_t currentTemp =
    checkMinMaxValues( &colorTemperatureMinimum, &colorTemperatureMaximum);
  uint16_t newTemp;

  switch(stepMode) {
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_STOP:
    stopColorTempTransition();
    break;
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_UP:

    newTemp = currentTemp + stepSize;

    if(newTemp > colorTemperatureMaximum)
      newTemp = colorTemperatureMaximum;

    kickOffTemperatureTransition( currentTemp, 
                                  newTemp, 
                                  transitionTime,
                                  currentEndpoint() );
    break;
  case EMBER_ZCL_COLOR_TEMP_MOVE_MODE_DOWN:

    newTemp = currentTemp - stepSize;

    if(newTemp < colorTemperatureMinimum ||
      stepSize > currentTemp )
      newTemp = colorTemperatureMinimum;

    kickOffTemperatureTransition( currentTemp, 
                                  newTemp, 
                                  transitionTime,
                                  currentEndpoint() );
    break;
  } 

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);

  return true;
}

bool emberAfColorControlClusterStopMoveStepCallback(void)
{
  stopColorTempTransition();

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);

  return true;
}

// **********************************************
// LED Output Blinking State

void emberAfPluginBulbPwmConfigurationBlinkStopCallback( uint8_t endpoint )
{
  updateDriveLevel( endpoint );
}

