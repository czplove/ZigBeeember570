// *******************************************************************
// * color-server.c
// *
// * Demonstration of a simple implementation of a color control server.  Note:
// * because this does not implement transitions, it is not HA certifiable.  
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/framework/plugin/led-rgb-pwm/led-rgb-pwm.h"

void emberAfPluginColorServerInitCallback( void )
{
  // Placeholder.
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
  uint8_t endpoint = emberAfCurrentEndpoint();
  uint8_t colorMode = 0x01;

  // need to write colorX and colorY here.  Note:  I am ignoring transition 
  // time for now.  
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorX,
                        ZCL_INT16U_ATTRIBUTE_TYPE);

  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorY,
                        ZCL_INT16U_ATTRIBUTE_TYPE);

  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorMode,
                        ZCL_INT8U_ATTRIBUTE_TYPE);

  // now we need to update the 
  emberAfLedRgbPwmComputeRgbFromXy( endpoint );

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return true;
}

/** @brief Move To Color Temperature
 *
 * 
 *
 * @param colorTemperature   Ver.: always
 * @param transitionTime   Ver.: always
 */
bool emberAfColorControlClusterMoveToColorTemperatureCallback(uint16_t colorTemperature,
                                                                 uint16_t transitionTime)
{
  uint8_t endpoint = emberAfCurrentEndpoint();
  uint8_t colorMode = 0x02;

  // need to write colorX and colorY here.  Note:  I am ignoring transition 
  // time for now.  
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorTemperature,
                        ZCL_INT16U_ATTRIBUTE_TYPE);
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorMode,
                        ZCL_INT8U_ATTRIBUTE_TYPE);

  emberAfLedRgbPwmComputeRgbFromColorTemp(endpoint);
 
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
  uint8_t endpoint = emberAfCurrentEndpoint();
  uint8_t colorMode = 0x00;

  // limit checking:  hue and saturation are 0..254
  if(hue == 255 || saturation == 255) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
    return true;
  }
  
  // need to write colorX and colorY here.  Note:  I am ignoring transition 
  // time for now.  
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&hue,
                        ZCL_INT8U_ATTRIBUTE_TYPE);
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&saturation,
                        ZCL_INT8U_ATTRIBUTE_TYPE);
  emberAfWriteAttribute(endpoint,
                        ZCL_COLOR_CONTROL_CLUSTER_ID,
                        ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *)&colorMode,
                        ZCL_INT8U_ATTRIBUTE_TYPE);

  emberAfLedRgbPwmComputeRgbFromHSV(endpoint);

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return true;
}
