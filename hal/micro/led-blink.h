// *******************************************************************
// * led-blink.h
// *
// * API to allow for basic control of activity LEDs, including on/off
// * functionality and blink pattern creation.
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *******************************************************************

// This code uses a number of  ::EmberEventControl events to properly function.
// The application framework will generally manage the event automatically. 
// Customers who do not use the framework must ensure the event is run, by 
// calling either ::emberRunEvents or ::emberRunTask.

#ifndef __LED_BLINK_H__
#define __LED_BLINK_H__

/** @brief Set the LED to blink
 *
 * @note The led blink API is currently limited to one LED.  This function will
 * set which LED should be designated as the activity LED.
 *
 * @param led  The led to set as the activity LED.  It is recommended this be
 * passed in either in by using the PORTx_PIN(y) macro.
 */
void halLedBlinkSetActivityLed(uint8_t led);

/** @brief Set the activity LED to blink a given number of times.
 *
 * @note This function will cause the activity LED to blink a user provided
 * number of times at a user provided rate.
 *
 * @param count  The number of times to blink the LED
 * @param blinkTime  The amount of time (in ms) the LED should be on and off for
 */
void halLedBlinkBlink( uint8_t count, uint16_t blinkTime);

/** @brief Cause the activity LED to blink in a specified pattern
 *
 * @note This function will cause the activity LED to blink a user specified
 * pattern.
 *
 * @param count  The number of times to cycle through the pattern
 * @param length  The number entries in the pattern array
 * @param pattern  An array of on and off times (in mS) that describes the
 * blink pattern.  The first integer must describe an on time (can be 0), but
 * the last integer is not required to be an off time.  Maximum pattern length
 * is 20 unsigned 16 bit entries.
 */
void halLedBlinkPattern( uint8_t count, uint8_t length, uint16_t *pattern);

/** @brief Turn the Activity LED off
 *
 * @note This function will turn the activity LED off.  If a value of 0 is
 * passed in, it will keep the LED off indefinitely.  Otherwise, it will turn
 * the LED back on after the specified amount of time has passed.
 *
 * @param time  The amount of time (in mS) to turn the LED off.  If 0, the LED
 * will remain off indefinitely.
 */
void halLedBlinkLedOff( uint8_t time );

/** @brief Turn the Activity LED on
 *
 * @note This function will turn the activity LED on.  If a value of 0 is
 * passed in, it will keep the LED on indefinitely.  Otherwise, it will turn
 * the LED back off after the specified amount of time has passed.
 *
 * @param time  The amount of time (in mS) to turn the LED on.  If 0, the LED
 * will remain on indefinitely.
 */
void halLedBlinkLedOn( uint8_t time );

/** @brief Clear a GPIO in a sleepy system
 *
 * @note This function will clear a bit for a sleepy system and ensure that it
 * remains cleared when the device enters and exits sleep.
 *
 * @param port is the GPIO port of the pin to be cleared.
 * @param pin is the GPIO pin to be cleared.
 */
void halLedBlinkSleepyClearGpio( uint8_t port, uint8_t pin );

/** @brief Set a GPIO in a sleepy system
 *
 * @note This function will clear a bit for a sleepy system and ensure that it
 * remains cleared when the device enters and exits sleep.
 *
 * @param port is the GPIO port of the pin to be cleared.
 * @param pin is the GPIO pin to be cleared.
 */
void halLedBlinkSleepySetGpio( uint8_t port, uint8_t pin );

#endif //__LED_BLINK_H__

