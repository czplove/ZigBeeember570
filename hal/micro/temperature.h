// *****************************************************************************
// * temperature.h
// *
// * API for reading temperature data from a temperature sensor
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

// Some implementations of this function may make use of ::EmberEventControl
// events.  If so, the application framework will generally manage the events
// automatically.  Customers who do not use the framework must ensure the events
// are run, by calling either ::emberRunEvents or ::emberRunTask.  See the
// specific implementation for more information.

#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

/** @brief Initiate a temperature read
 *
 * This function will initiate a read of the temperature sensor.  This read will
 * then trigger a driver specific callback when the read has completed.
 *
 * @return 0 if no errors were encountered on read, a driver specific error
 * code otherwise.
 */
uint8_t halTemperatureStartRead(void);

/** @brief Initializes the temperature hardware, along with any hardware
 * peripherals necessary to interface with the hardware.  The application
 * framework will generally initialize this plugin automatically.  Customers who
 * do not use the framework must ensure the plugin is initialized by calling
 * this function.
 */
void halTemperatureInit(void);

#endif //__TEMPERATURE_H__
