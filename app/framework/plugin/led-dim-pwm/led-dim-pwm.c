// *******************************************************************
// * led-dim-pwm.c
// *
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

#include "led-dim-pwm-transform.h"

static uint8_t minLevel;
static uint8_t maxLevel;
static uint16_t minPwmDrive, maxPwmDrive;

// Use precalculated values for the PWM based on a 1 kHz frequency to achieve
// the proper perceived LED output.  
uint16_t pwmValues[] = { PWM_VALUES };

static void updateDriveLevel( uint8_t endpoint);
void emberAfPluginLedDimPwmInitCallback( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;

  halInternalPowerUpBoard(); // redoing this here just in case.

  minPwmDrive = emberAfPluginBulbConfigMinDriveValue();
  maxPwmDrive = emberAfPluginBulbConfigMaxDriveValue();

  // Set the min and max levels
#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  minLevel = EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER_MINIMUM_LEVEL;
  maxLevel = EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER_MAXIMUM_LEVEL;
#else
  minLevel = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
  maxLevel = EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL;
#endif

  updateDriveLevel(fixedEndpoints[0]);
}

static void pwmSetValue( uint16_t value )
{
  emberAfPluginBulbConfigDrivePwm(value);
}

// update drive level based on linear power delivered to the light
static uint16_t updateDriveLevelLumens( uint8_t endpoint)
{
  uint32_t driveScratchpad;
  uint8_t currentLevel, mappedLevel;
  uint16_t newDrive;
  
  emberAfReadServerAttribute(endpoint,
                             ZCL_LEVEL_CONTROL_CLUSTER_ID,
                             ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                             (uint8_t *)&currentLevel,
                             sizeof(currentLevel));

  // First handle the degenerate case.  
  if(currentLevel == 0)
    return 0;

  // first, map the drive level into the size of the table
  // We have a 255 entry table that goes from 0 to 6000.
  // use 32 bit math to avoid losing information.
  driveScratchpad = currentLevel - minLevel;
  driveScratchpad *= PWM_VALUES_LENGTH;
  driveScratchpad /= (maxLevel - minLevel);
  mappedLevel = (uint8_t) driveScratchpad;

  driveScratchpad = (uint32_t) pwmValues[ mappedLevel ];

  // newDrive now is mapped to 0..6000.  We need to remap it 
  // to WHITE_MIMIMUM_ON_VALUE..WHITE_MAXIMUM_ON_VALUE
  // use 32 bit math to avoid losing information.
  driveScratchpad = driveScratchpad * (maxPwmDrive-minPwmDrive);
  driveScratchpad = driveScratchpad / 6000;
  driveScratchpad += minPwmDrive;
  newDrive = (uint16_t) driveScratchpad;

  return newDrive;
}  

static void updateDriveLevel( uint8_t endpoint)
{
  bool isOn;

  // updateDriveLevel is called before maxLevel has been initialzied.  So if
  // maxLevel is zero, we need to set the PWM to 0 for now and exit.  This 
  // will be called again after maxLevel has been initialzied.  
  if(maxLevel == 0) {

    pwmSetValue( 0 );
    return;
  }

  emberAfReadServerAttribute(endpoint,
                             ZCL_ON_OFF_CLUSTER_ID,
                             ZCL_ON_OFF_ATTRIBUTE_ID,
                             (uint8_t *)&isOn,
                             sizeof(bool));

  if(isOn) {
    pwmSetValue( updateDriveLevelLumens( endpoint ) );
  } else {
    pwmSetValue(0);
  }
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
  updateDriveLevel( endpoint );
}

void emberAfOnOffClusterServerAttributeChangedCallback(uint8_t endpoint, 
                                                       EmberAfAttributeId attributeId)
{
  updateDriveLevel( endpoint );
}

void emAfLedDimPwmTestCommand(void)
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  if(pwmValue > 0 && pwmValue < minPwmDrive)
    pwmValue = minPwmDrive;

  if(pwmValue > maxPwmDrive)
    pwmValue = maxPwmDrive;

  emberAfAppPrintln("White PWM Value:  %2x",pwmValue);

  pwmSetValue( pwmValue );

}

// **********************************************
// LED Output Blinking State

void emberAfPluginBulbPwmConfigurationBlinkStopCallback( uint8_t endpoint )
{
  updateDriveLevel( endpoint );
}

