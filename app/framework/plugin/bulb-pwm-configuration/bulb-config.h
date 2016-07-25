// *******************************************************************
// * bulb-config.h
// *
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.            *80*
// *******************************************************************

#ifndef __BULB_CONFIG_H__
#define __BULB_CONFIG_H__

#include "../../include/af.h"
#include "hal/micro/micro.h"

#include PLATFORM_HEADER
#include "include/error.h"
#include "hal/micro/cortexm3/flash.h"

#define EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP CIB_TOP

#define EMBER_AF_PLUGIN_BULB_CONFIG_MASK_LOCATION             \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 1)   // 0xffe
#define EMBER_AF_PLUGIN_BULB_CONFIG_TX_POWER_LOCATION         \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 3)   // 0xffc
#define EMBER_AF_PLUGIN_BULB_CONFIG_FREQ_LOCATION             \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 5)   // 0xffa
#define EMBER_AF_PLUGIN_BULB_CONFIG_MIN_ON_LOCATION           \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 7)   // 0xff8
#define EMBER_AF_PLUGIN_BULB_CONFIG_MAX_ON_LOCATION           \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 9)   // 0xff6
#define EMBER_AF_PLUGIN_BULB_CONFIG_PWM_WHITE_LOCATION        \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 11)  // 0xff4
#define EMBER_AF_PLUGIN_BULB_CONFIG_PWM_RED_LOCATION          \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 13)  // 0xff2
#define EMBER_AF_PLUGIN_BULB_CONFIG_PWM_GREEN_LOCATION        \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 15)  // 0xff0
#define EMBER_AF_PLUGIN_BULB_CONFIG_PWM_BLUE_LOCATION         \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 17)  // 0xfee
#define EMBER_AF_PLUGIN_BULB_CONFIG_HARDWARE_VERSION_LOCATION \
        (EMBER_AF_PLUGIN_BULB_CONFIG_LOCATION_TOP - 19)  // 0xfec

#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_TX_POWER_BIT          BIT(0)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_FREQUENCY_BIT         BIT(1)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_MIN_ON_BIT            BIT(2)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_MAX_ON_BIT            BIT(3)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_WHITE_PWM_BIT         BIT(4)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_RED_PWM_BIT           BIT(5)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_GREEN_PWM_BIT         BIT(6)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_BLUE_PWM_BIT          BIT(7)
#define EMBER_AF_PLUGIN_BULB_CONFIG_HAS_HARDWARE_VERSION_BIT  BIT(8)

#define EMBER_AF_PLUGIN_BULB_CONFIG_PORT_A  0
#define EMBER_AF_PLUGIN_BULB_CONFIG_PORT_B  1
#define EMBER_AF_PLUGIN_BULB_CONFIG_PORT_C  2

#define EMBER_AF_PLUGIN_BULB_CONFIG_UNDEFINED 0xFF

/** @brief Returns the bitmask of which bulb configuration parameters have been
 *  set.
 *
 * Function to return a bitmask that details which of the bulb configuration 
 * parameters have been set.  
 *
 * @return An ::uint16_t value bitmask that shows which parameters where set.  A
 * value of 1 measn not set, and a value of 0 means set.  
 */
uint16_t emberAfBulbConfigMask( void );

/** @brief Returns configured value for transmitm power.  
 *
 * This function will read the CIB area specified by the bulb config plugin 
 * for the TX power required by this device.  Note:  this function does not 
 * check to see if the value was valid.  The valid value must be checked from 
 * the function emberAfBulbConfigHasTxPower().
 *
 * @return An ::int8_t value with the preferred transmit power for this
 * hardware.
 */
int8_t  emberAfBulbConfigTxPower( void );

/** @brief PWM Frequency for the bulb drive PWMs.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the PWM frequency, in HZ.  Note:  this function does not check to see
 * if the value was valid.  The value must be checked with the function 
 * emberAfBulbConfigHasFrequency().
 *
 * @return An ::uint16_t value that specifies the PWM frequency in Hz.
 */
uint16_t emberAfBulbConfigFrequency( void );

/** @brief Specify minimum on time in microseconds.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the minimum on time in microseconds.  Note:  this function does not 
 * check to see if the value was valid.  The value must be chedked with the 
 * function emberAfBulbConfigHasMinOnUs().
 *
 * @return An ::uint16_t value that specifies the minimum on time in 
 * microseconds.
 */
uint16_t emberAfBulbConfigMinOnUs( void );

/** @brief Specify maximum on time in microseconds.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the maximum on time in microseconds.  Note:  this function does not 
 * check to see if the value was valid.  The value must be chedked with the 
 * function emberAfBulbConfigHasMaxOnUs().
 *
 * @return An ::uint16_t value that specifies the maximum on time in 
 * microseconds.
 */
uint16_t emberAfBulbConfigMaxOnUs( void );

/** @brief Port and pin for the white PWM.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the GPIO port and pin of the white PWM output.  The port is specified 
 * by bits 3 and 4, while the pin is specified by bits 0 to 2.  Note:  this 
 * function does not check to see if the value was valid.  The value must be
 * checked with the function emberAfBulbConfigHasWhitePwm().
 *
 * @return An ::uint8_t value that specifies the port and pin of the white PWM.
 */
uint8_t  emberAfBulbConfigPwmWhitePort( void );

/** @brief Port and pin for the red PWM.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the GPIO port and pin of the red PWM output.  The port is specified 
 * by bits 3 and 4, while the pin is specified by bits 0 to 2.  Note:  this 
 * function does not check to see if the value was valid.  The value must be
 * checked with the function emberAfBulbConfigHasRedPwm().
 *
 * @return An ::uint8_t value that specifies the port and pin of the red PWM.
 */
uint8_t  emberAfBulbConfigPwmRedPort( void );

/** @brief Port and pin for the green PWM.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the GPIO port and pin of the green PWM output.  The port is specified 
 * by bits 3 and 4, while the pin is specified by bits 0 to 2.  Note:  this 
 * function does not check to see if the value was valid.  The value must be
 * checked with the function emberAfBulbConfigHasGreenPwm().
 *
 * @return An ::uint8_t value that specifies the port and pin of the green PWM.
 */
uint8_t  emberAfBulbConfigPwmGreenPort( void );

/** @brief Port and pin for the blue PWM.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the GPIO port and pin of the blue PWM output.  The port is specified 
 * by bits 3 and 4, while the pin is specified by bits 0 to 2.  Note:  this 
 * function does not check to see if the value was valid.  The value must be
 * checked with the function emberAfBulbConfigHasBluePwm().
 *
 * @return An ::uint8_t value that specifies the port and pin of the blue PWM.
 */
uint8_t  emberAfBulbConfigPwmBluePort( void );

/** @brief Returns the hardware version.
 *
 * This function will read the CIB area specified by the bulb config plugin
 * for the hardware version.  Note:  this function does not check to see if
 * the value is valid.  The value must be checked with the function 
 * emberAfBulbconfigHasHardwareVersion();
 *
 * @return An ::uint8_t value that represents the hardware version.
 */
uint8_t  emberAfBulbConfigHardwareVersion( void );

/** @brief Function to return whether the TX power was configured.
 *
 * This function returns true if the TX power was configured, and false if
 * not.  
 *
 * @return A ::bool value stating whether the TX power was configured.
 */
bool emberAfBulbConfigHasTxPower( void );

/** @brief Function to return whether the PWM frequency was configured.
 *
 * This function returns true if the PWM frequency was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the PWM frequency was configured.
 */
bool emberAfBulbConfigHasFrequency( void );

/** @brief Function to return whether the min on time was configured.
 *
 * This function returns true if the min on time was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the min on time was configured.
 */
bool emberAfBulbConfigHasMinOnUs( void );

/** @brief Function to return whether the max on time was configured.
 *
 * This function returns true if the max on time was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the max on time was configured.
 */
bool emberAfBulbConfigHasMaxOnUs( void );

/** @brief Function to return whether the white PWM was configured.
 *
 * This function returns true if the white PWM was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the white PWM was configured.
 */
bool emberAfBulbConfigHasWhitePwm( void );

/** @brief Function to return whether the red PWM was configured.
 *
 * This function returns true if the red PWM was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the red PWM was configured.
 */
bool emberAfBulbConfigHasRedPwm( void );

/** @brief Function to return whether the green PWM was configured.
 *
 * This function returns true if the green PWM was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the green PWM was configured.
 */
bool emberAfBulbConfigHasGreenPwm( void );

/** @brief Function to return whether the blue PWM was configured.
 *
 * This function returns true if the blue PWM was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the blue PWM was configured.
 */
bool emberAfBulbConfigHasBluePwm( void );

/** @brief Function to return whether the hardware version was configured.
 *
 * This function returns true if the hardware version was configured, and 
 * false if not.  
 *
 * @return A ::bool value stating whether the hardware version was 
 * configured.
 */
bool emberAfBulbConfigHasHardwareVersion( void );

/** @brief Port number for the white PWM
 *
 * Function to return the white port number.  It retursn an enum 0, 1, or 2 
 * for port a, b, or c respectively.  It will return 0xff if the port has
 * not been configured.
 *
 * @return An ::uint8_t value for the white port.
 */
uint8_t emberAfBulbConfigWhitePort( void );

/** @brief Pin number for the white PWM
 *
 * Function to return the white pin number.  It retursn a value between 0 and
 * 7 inclusive corresponding to the pin number of the white PWM.  It will return 
 * 0xff if the pin has not been configured.
 *
 * @return An ::uint8_t value for the white pin.
 */
uint8_t emberAfBulbConfigWhitePin( void );

/** @brief Port number for the red PWM
 *
 * Function to return the red port number.  It retursn an enum 0, 1, or 2 
 * for port a, b, or c respectively.  It will return 0xff if the port has
 * not been configured.
 *
 * @return An ::uint8_t value for the red port.
 */
uint8_t emberAfBulbConfigRedPort( void );

/** @brief Pin number for the red PWM
 *
 * Function to return the red pin number.  It retursn a value between 0 and
 * 7 inclusive corresponding to the pin number of the red PWM.  It will return 
 * 0xff if the pin has not been configured.
 *
 * @return An ::uint8_t value for the red pin.
 */
uint8_t emberAfBulbConfigRedPin( void );

/** @brief Port number for the green PWM
 *
 * Function to return the green port number.  It retursn an enum 0, 1, or 2 
 * for port a, b, or c respectively.  It will return 0xff if the port has
 * not been configured.
 *
 * @return An ::uint8_t value for the green port.
 */
uint8_t emberAfBulbConfigGreenPort( void );

/** @brief Pin number for the green PWM
 *
 * Function to return the green pin number.  It retursn a value between 0 and
 * 7 inclusive corresponding to the pin number of the green PWM.  It will return 
 * 0xff if the pin has not been configured.
 *
 * @return An ::uint8_t value for the green pin.
 */
uint8_t emberAfBulbConfigGreenPin( void );

/** @brief Port number for the blue PWM
 *
 * Function to return the blue port number.  It retursn an enum 0, 1, or 2 
 * for port a, b, or c respectively.  It will return 0xff if the port has
 * not been configured.
 *
 * @return An ::uint8_t value for the blue port.
 */
uint8_t emberAfBulbConfigBluePort( void );

/** @brief Pin number for the blue PWM
 *
 * Function to return the blue pin number.  It retursn a value between 0 and
 * 7 inclusive corresponding to the pin number of the blue PWM.  It will return 
 * 0xff if the pin has not been configured.
 *
 * @return An ::uint8_t value for the blue pin.
 */
uint8_t emberAfBulbConfigBluePin( void );

/** @brief Pointer to the white compare register.
 *
 * This function will examine the bulb configuration and return a pointer to
 * the white compare register.  It will return a pointer to the defualt
 * register as specified in the board header file if none has been configured
 * through the bulb config plugin.  
 *
 * @return An ::uint32_t* pointer to the white PWM compare register.
 */
volatile uint32_t *emberAfPwmControlWhitePwm( void );

/** @brief Pointer to the red compare register.
 *
 * This function will examine the bulb configuration and return a pointer to
 * the red compare register.  It will return a pointer to the defualt
 * register as specified in the board header file if none has been configured
 * through the bulb config plugin.  
 *
 * @return An ::uint32_t* pointer to the red PWM compare register.
 */
volatile uint32_t *emberAfPwmControlRedPwm( void );

/** @brief Pointer to the green compare register.
 *
 * This function will examine the bulb configuration and return a pointer to
 * the green compare register.  It will return a pointer to the defualt
 * register as specified in the board header file if none has been configured
 * through the bulb config plugin.  
 *
 * @return An ::uint32_t* pointer to the green PWM compare register.
 */
volatile uint32_t *emberAfPwmControlGreenPwm( void );

/** @brief Pointer to the white blue register.
 *
 * This function will examine the bulb configuration and return a pointer to
 * the blue compare register.  It will return a pointer to the defualt
 * register as specified in the board header file if none has been configured
 * through the bulb config plugin.  
 *
 * @return An ::uint32_t* pointer to the blue PWM compare register.
 */
volatile uint32_t *emberAfPwmControlBluePwm( void );

/** @brief Return the minimum valid value for the PWM.
 *
 * This function will examine the configuration values and return the minimum
 * valid PWM value when it is on.  Note:  the PWM will still be set to zero 
 * for an off condition.  However, values between zero and the minimum drive
 * are invalid as they may damage hardware or yield unpredictable flashing
 * behavior on the LED output.
 *
 * @return An ::uint16_t value that is the minimum drive level for the PWM when 
 * on.
 */
uint16_t emberAfPluginBulbConfigMinDriveValue( void );

/** @brief Return the maximum valid value for the PWM.
 *
 * This function will examine the configuration values and return the maximum
 * valid PWM value when it is on.  Note:  if the max on microseconds is not
 * configured, this value will be equal to the PWM tics per period value.  
 *
 * @return An ::uint16_t value that is the maximum drive level for the PWM when 
 * on.
 */
uint16_t emberAfPluginBulbConfigMaxDriveValue( void );

/** @brief Return the tics per PWM period..
 *
 * This function will examine the frequency configuration and determine the 
 * number of PWM ticks required to implement that frequency.  
 *
 * @return An ::uint16_t value that is the number of PWM ticks per period.
 */
uint16_t emberAfBulbConfigTicsPerPeriod( void );

/** @brief Initialize PWM output hardware based on configuration.
 *
 * This funnction will examine the bulb configuration values and configure
 * the PWM hardware to match those settings.  
 *
 */
void emberAfPluginBulbConfigConfigurePwm( void );

/** @brief Function to drive the white, red, green, and blue PWM outputs.
 *
 * This function will set the compare registers for the white, red, green,
 * and blue PWM drivers.  
 *
 * @param white:  value for the white comparison register.
 *
 * @param red:  value for the red comparison register.
 *
 * @param green:  value for the green comparison register.
 *
 * @param blue:  value for the blue comparison register.
 *
 */
void emberAfPluginBulbConfigDriveWRGB( uint16_t white, 
                                       uint16_t red, 
                                       uint16_t green, 
                                       uint16_t blue );

/** @brief Function to drive the white PWM output.
 *
 * This function will set the compare registers for the white pwm output.  It
 * will automatically set the values for red, green, and blue to zero.  It is 
 * intended for a simple, white dimmable bulb.  
 *
 * @param value:  value for the white comparison register.
 *
 */
void emberAfPluginBulbConfigDrivePwm( uint16_t value );

/** @brief Function to turn on the LED output.  
 *
 * Function to turn the LED on full brightness as an indication to the 
 * user.  After the time, the LED will be reset to the appropriate values
 * as determined by the level, on/off, and color control cluster (if
 * appropriate).  
 *
 * @param time:  Number of seconds to turn the LED on.  0 means forever.
 */
void emberAfPluginBulbConfigLedOn( uint8_t time );

/** @brief Function to turn off the LED output.  
 *
 * Function to turn the LED off as an indication to the user.  After the time, 
 * the LED will be reset to the appropriate values as determined by the level, 
 * on/off, and color control cluster (if appropriate).  
 *
 * @param time:  Number of seconds to turn the LED off.  0 means forever.
 */
void emberAfPluginBulbConfigLedOff( uint8_t time );

/** @brief Blink the LED.
 *
 * Function to blink the LED as an indication to the user.  Note:  this will
 * blink the LED symmetrically.  If asymmetric blinking is required, please 
 * use the function ::emberAfPluginBulbConfigLedBlinkPattern(...).
 *
 * @param count:  Number of times to blink.  0 means forever.  
 *
 * @param blinkTime:  Amount of time the bulb will be on or off during the
 * blink.
 */
void emberAfPluginBulbConfigLedBlink( uint8_t count, uint16_t blinkTime );

/** @brief Blink a pattern on the LED.
 *
 * Function to blink a pattern on the LED.  User sets up a pattern of on/off
 * events.  
 * 
 * @param count:  Number of times to blink the pattern.  0 means forever
 * 
 * @param length:  Length of the pattern.  20 is the maximum lenght.
 *
 * @param pattern[]:  Series of on/off times for the blink pattern.  
 *
 */
void emberAfPluginBulbConfigLedBlinkPattern( uint8_t count, uint8_t length, uint16_t *pattern );

/** @brief Dim the bulb on/off.
 *
 * Function to dim the bulb on and off.  This is an option disabled by default
 * but requested by several customers.  Instead of transitioning from on to off
 * immediately, this function creates a slower transition from on to off and 
 * back to on over the course of ::time milliseconds.  
 * 
 * @param count:  Number of times to blink.  0 means forever
 * 
 * @param timeMs:  Length time, in milliseconds, for the transition.
 *
 */
void emberAfBulbConfigDimIndicate(uint8_t count, uint16_t timeMs);

#endif
