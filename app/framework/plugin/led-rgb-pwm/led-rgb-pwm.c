// *******************************************************************
// * led-rgb-pwm.c
// *
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *******************************************************************
#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "temp-to-rgb.h"

#ifdef EMBER_AF_PLUGIN_SCENES
  #include "app/framework/plugin/scenes/scenes.h"
#endif //EMBER_AF_PLUGIN_SCENES

#ifdef EMBER_AF_PLUGIN_ON_OFF
  #include "app/framework/plugin/on-off/on-off.h"
#endif //EMBER_AF_PLUGIN_ON_OFF

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  #include "app/framework/plugin/zll-level-control-server/zll-level-control-server.h"
#endif //EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER

#ifdef EMBER_AF_PLUGIN_BULB_CONFIG
  #include "app/framework/plugin/bulb-config/bulb-config.h"
#endif

static uint8_t minLevel;
static uint8_t maxLevel;

static uint16_t minPwmDrive, maxPwmDrive;

// Use precomputed values for RED, GREEN and BLUE based on algorithm by
// Robertson.
uint8_t tempRedValue[]   = { RED_VALUES };
uint8_t tempGreenValue[] = { GREEN_VALUES };
uint8_t tempBlueValue[]  = { BLUE_VALUES };

#define PWM_POLARITY      EMBER_AF_PLUGIN_PWM_CONTROL_PWM_POLARITY
#define ON_OFF_OUTPUT     EMBER_AF_PLUGIN_PWM_CONTROL_ON_OFF_OUTPUT

static void updateDriveLevel( uint8_t endpoint);
void emberAfPluginLedRgbPwmInitCallback( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;
  // put PWM initialization code here.

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

static void driveWRGB( uint16_t white, uint16_t red, uint16_t green, uint16_t blue )
{
  emberAfPluginBulbConfigDriveWRGB( white, red, green, blue );
}

// apply RGB transform to XY input
// Note:  X and Y are well-known quantities in the color space.  It comes
// from the CIE xyY color model.  Here, currentX and currentY are the ZCL
// attributes where x = currentX / 65536 and y = currentY / 65536.  
// http://en.wikipedia.org/wiki/CIE_1931_color_space for more details
void emberAfLedRgbPwmComputeRgbFromXy( uint8_t endpoint )
{
  uint16_t currentX, currentY;
  uint8_t onOff, currentLevel;
  
  uint32_t scratch;
  uint32_t X32, Y32, Z32;
  int32_t R32, G32, B32;
  uint16_t rDrive, gDrive, bDrive;

  // read the attributes from the attribute table.  
  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
                             (uint8_t *)&currentX,
                             sizeof(currentX));
  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
                             (uint8_t *)&currentY,
                             sizeof(currentY));
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

  // compute x, y, z
  X32 = currentX;
  Y32 = currentY;

  scratch = X32 + Y32;

  if(scratch > 65536l) {
    emberAfAppPrintln("X and Y are too big");
    return;
  }

  Z32 = 65536l - (X32 + Y32);

  //emberAfAppPrintln("x: %d, y: %d, z: %d\r\n", (uint16_t) X32, (uint16_t) Y32, (uint16_t) Z32);

  // now we can compute the RGB values in 65,536,000
  // these are well-known constants but are documented at: 
  // http://docs-hoffmann.de/ciexyz29082000.pdf which came from
  // "Digital Color Management, Giorgianni+Madden
  R32 = (X32 * 2365) - (Y32 * 897) - (Z32 * 468);
  G32 = ((Y32 * 1426) + (Z32 * 89)) - (X32 * 515);
  B32 = (X32 * 5) + (Z32 * 1009) - (Y32 * 14);

  R32 = R32 / 65536;
  R32 = R32 * maxPwmDrive;
  R32 = R32 / 1000;
  rDrive = (uint16_t) R32;

  G32 = G32 / 65536;
  G32 = G32 * maxPwmDrive;
  G32 = G32 / 1000;
  gDrive = (uint16_t) G32;

  B32 = B32 / 65536;
  B32 = B32 * maxPwmDrive;
  B32 = B32 / 1000;
  bDrive = (uint16_t) B32;

  //emberAfAppPrintln("R: %d, G: %d, B: %d\r\n", rDrive, gDrive, bDrive);

  // limits checking.  Also, handle level.
  if(rDrive > 32768)
    rDrive = 0;
  else {
    R32 *= currentLevel;
    R32 /= 256;
    rDrive = (uint16_t) R32;
  }
  if(gDrive > 32768)
    gDrive = 0;
  else {
    G32 *= currentLevel;
    G32 /= 256;
    gDrive = (uint16_t) G32;
  }
  if(bDrive > 32768)
    bDrive = 0;
  else {
    B32 *= currentLevel;
    B32 /= 256;
    bDrive = (uint16_t) B32;
  }

  if(rDrive > maxPwmDrive)
    rDrive = maxPwmDrive;
  if(gDrive > maxPwmDrive)
    gDrive = maxPwmDrive;
  if(bDrive > maxPwmDrive)
    bDrive = maxPwmDrive;

  //emberAfAppPrintln("R: %d, G: %d, B: %d\r\n", rDrive, gDrive, bDrive);

  driveWRGB(0, rDrive, gDrive, bDrive);
}

void emberAfLedRgbPwmComputeRgbFromColorTemp( uint8_t endpoint )
{
  uint16_t currentTemp;
  uint8_t onOff, currentLevel;

  uint32_t R32, G32, B32, W32;
  uint16_t rDrive, gDrive, bDrive, wDrive;

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

  // temperature is 1,000,000/currentTemperature.  
  // I computed R,G,B from a simple algorithm from Robertson and created
  // tables.
  currentTemp = currentTemp / 10;
  if(currentTemp > 99) {
    currentTemp = 99;
  }

  // red, green, blue values are in 0..255.  Just add white at the max.
  W32 = 0;
  R32 = tempRedValue[currentTemp];
  G32 = tempGreenValue[currentTemp];
  B32 = tempBlueValue[currentTemp];

  // handle conversion to ticks and level control.
  W32 = W32 * maxPwmDrive;
  R32 = R32 * maxPwmDrive;
  G32 = G32 * maxPwmDrive;
  B32 = B32 * maxPwmDrive;

  // now we have white, red, green, and blue in 0 to 255 * TICS_PER_PERIOD.
  // level is 0.255.  To convert to drive, we multiply by level and divide by
  // 255 * 255.  
  R32 = R32 * currentLevel;
  R32 = R32 / (255 * 255);
  G32 = G32 * currentLevel;
  G32 = G32 / (255 * 255);
  B32 = B32 * currentLevel;
  B32 = B32 / (255 * 255);
  W32 = W32 * currentLevel;
  W32 = W32 / (255 * 255);

  // convert to uint16_t and drive the PWMs.
  rDrive = (uint16_t) R32;
  gDrive = (uint16_t) G32;
  bDrive = (uint16_t) B32;
  wDrive = (uint16_t) W32;

  driveWRGB(wDrive, rDrive, gDrive, bDrive);
}

void emberAfLedRgbPwmComputeRgbFromHSV( uint8_t endpoint )
{
  uint8_t hue, saturation;
  uint8_t onOff, currentLevel;

  uint32_t min32, hue32, delta32, sat32, level32;

  uint32_t R32, G32, B32;
  uint16_t rDrive, gDrive, bDrive;

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID,
                             (uint8_t *)&hue,
                             sizeof(hue));

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID,
                             (uint8_t *)&saturation,
                             sizeof(saturation));

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

  // algorithm taken from wikipedia
  // http://en.wikipedia.org/wiki/CIE_1931_color_space for more details

  // note:  hue and saturation are 0..254.  level (i.e. value for HSV) is 
  // 0..255.  But most of these computations assume 0..1 for saturation and 
  // value, and 0..360 for hue.  This will be a little tricky to compute RGB 
  // using fixed point math and not lose any bits of significance.  

  // first switch to 32 bit mode.  
  level32 = (uint32_t) currentLevel;
  sat32 = (uint32_t) saturation;

  min32 = level32 * (254 - sat32);
  min32 = min32 / 254;
  delta32 = level32 - min32;
  hue32 = ((uint32_t) hue);  // need to map it to 0..6.  really is 0..254.

  // formula is X = C * { 1 - ( H mod2 - 1) }.  Becuase the nubmers don't line 
  // up, we need to do this with if's.
  // The ranges are 0, 42, 84, 127, 169, 211, 254.
  if(hue < 43) {
    R32 = level32; // 0..254
    // convert 0..42 to 0..delta32
    G32 = hue32 * delta32;
    G32 = G32 / 42;
    G32 = G32 + min32;
    B32 = min32;
  } else if(hue < 85) {
    hue32 -= 42;
    // convert 0..42 to delta32..0
    R32 = (42 - hue32) * delta32;
    R32 = R32 / 42;
    R32 = R32 + min32;
    G32 = level32;
    B32 = min32;
  } else if(hue < 128) {
    hue32 -= 84;
    R32 = min32;
    G32 = level32;
    // convert 0..43 to 0..delta32
    B32 = hue32 * delta32;
    B32 = B32 / 43;
    B32 = B32 + min32;
  } else if(hue < 170) {
    hue32 -= 127;
    R32 = min32;
    // convert 0..42 to delta32..0
    G32 = (42 - hue32) * delta32;
    G32 = G32 / 42;
    G32 = G32 + min32;
    B32 = level32;  
  } else if(hue < 212) {
    hue32 -= 169;
    // convert 0..42 to 0..delta32
    R32 = hue32 * delta32;
    R32 = R32 / 42;
    R32 = R32 + min32;
    G32 = min32;
    B32 = level32;
  } else { //hue is 212..254
    hue32 -= 211;
    R32 = level32;
    G32 = min32;
    // convert 0..43 to delta32..0
    B32 = (43 - hue32) * delta32;
    B32 = B32 / 42;
    B32 = B32 + min32;
  }

  R32 = R32 * maxPwmDrive;
  G32 = G32 * maxPwmDrive;
  B32 = B32 * maxPwmDrive;
  R32 = R32 /254;
  G32 = G32 /254;
  B32 = B32 /254;

  rDrive = (uint16_t) R32;
  gDrive = (uint16_t) G32;
  bDrive = (uint16_t) B32;

  driveWRGB(0, rDrive, gDrive, bDrive);
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
  uint8_t colorMode;

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                             (uint8_t *)&colorMode,
                             sizeof(colorMode));

  switch(colorMode) {
    case 0x00:  // hue and saturation
     emberAfLedRgbPwmComputeRgbFromHSV(endpoint);
     break;
    case 0x01:  // XY
      emberAfLedRgbPwmComputeRgbFromXy( endpoint );
      break;
    case 0x02:  // color temperature
      emberAfLedRgbPwmComputeRgbFromColorTemp( endpoint );
      break;
  }     
}

void emberAfOnOffClusterServerAttributeChangedCallback(uint8_t endpoint, 
                                                       EmberAfAttributeId attributeId)
{
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, attributeId );
}

static void updateDriveLevel( uint8_t endpoint )
{
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, 0x0000 );
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
  emberAfLevelControlClusterServerAttributeChangedCallback( endpoint, 0x0000 );
}
                                                            

// **********************************************
// CLI Commands section
void emberAfPluginLedRgbPwmRgbXyyCommand( void )
{
  uint16_t currentX = (uint16_t)emberUnsignedCommandArgument(0);
  uint16_t currentY = (uint16_t)emberUnsignedCommandArgument(1);

  emberAfColorControlClusterMoveToColorCallback(currentX, currentY, 0);

}

// **********************************************
// LED Output Blinking State

void emberAfPluginBulbPwmConfigurationBlinkStopCallback( uint8_t endpoint )
{
  updateDriveLevel( endpoint );
}

