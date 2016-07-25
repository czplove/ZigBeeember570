// *******************************************************************
// * bulb-config-cli.c
// *
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.            *80*
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

void bulbConfigPwmPwm1( void )
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfAppPrintln("PWM1 Value:  %2x",pwmValue);

  emberAfPluginBulbConfigDriveWRGB(pwmValue, 0, 0, 0);
}

void bulbConfigPwmPwm2( void )
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfAppPrintln("PWM2 Value:  %2x",pwmValue);

  emberAfPluginBulbConfigDriveWRGB(0, pwmValue, 0, 0);
}

void bulbConfigPwmPwm3( void )
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfAppPrintln("PWM3 Value:  %2x",pwmValue);

  emberAfPluginBulbConfigDriveWRGB(0,0,pwmValue,0);
}

void bulbConfigPwmPwm4( void )
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfAppPrintln("PWM4 Value:  %2x",pwmValue);

  emberAfPluginBulbConfigDriveWRGB(0,0,0,pwmValue);
}

void bulbConfigPwmRGB( void )
{
  uint16_t pwmValueRed   = (uint16_t)emberUnsignedCommandArgument(0);
  uint16_t pwmValueGreen = (uint16_t)emberUnsignedCommandArgument(1);
  uint16_t pwmValueBlue  = (uint16_t)emberUnsignedCommandArgument(2);

  emberAfPluginBulbConfigDriveWRGB(0, pwmValueRed, pwmValueGreen, pwmValueBlue);
}

void bulbConfigPwmWhite( void )
{
  uint16_t pwmValue = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfAppPrintln("White Value:  %2x",pwmValue);

  emberAfPluginBulbConfigDriveWRGB(pwmValue, 0,0,0);
}
