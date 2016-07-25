// *******************************************************************
// * bulb-pwm-config.c
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

// ---------- stuff to go into board.h file ----------
// PWM and CLOCK setup
// Inputs

#ifndef PWM_WHITE 
  // Ideally, this is specified in the board.h file.  If it is not, we are 
  // assuming the customer is using our reference design.  

  // define frequency for timers...this goes to board.h
  #define TIM1_FREQ_HZ 0    // 0 means the timer is not used.
  #define TIM2_FREQ_HZ 1000 // 1 kHz.

  #define TIM2_CHAN1_ALT 1 // PA0 = 0, PB1 = 1
  #define TIM2_CHAN2_ALT 1 // PA1 = 0, PB2 = 1
  #define TIM2_CHAN3_ALT 1 // PA2 = 0, PB3 = 1
  #define TIM2_CHAN4_ALT 1 // PA3 = 0, PB4 = 1

  #define TIM1_CHAN1_ENABLE 0  // PB6
  #define TIM1_CHAN2_ENABLE 0  // PB7
  #define TIM1_CHAN3_ENABLE 0  // PA6
  #define TIM1_CHAN4_ENABLE 0  // PA7
  #define TIM2_CHAN1_ENABLE 1  // PA0/PB1
  #define TIM2_CHAN2_ENABLE 1  // PA1/PB2
  #define TIM2_CHAN3_ENABLE 1  // PA2/PB3
  #define TIM2_CHAN4_ENABLE 1  // PA3/PB4

  #define PWM_WHITE TIM2_CCR1

  #define CLOCK_FREQUENCY    6000000
  #define MINIMUM_ON_TIME_US 15
  #define MAXIMUM_ON_TIME_US (CLOCK_FREQUENCY / TIM2_FREQ_HZ * 6)
#endif

// Handle case for non-RGB light
#ifndef PWM_RED
  #define PWM_RED   TIM2_CCR2
  #define PWM_GREEN TIM2_CCR3
  #define PWM_BLUE  TIM2_CCR4
#endif

// Computed Values
#define TIM1_TICS_PER_PERIOD (CLOCK_FREQUENCY / TIM1_FREQ_HZ) 
#define TIM1_MINIMUM_ON_VALUE ( (MINIMUM_ON_TIME_US * 6) )
#define TIM1_MAXIMUM_ON_VALUE ( (MAXIMUM_ON_TIME_US * 6) )

#define TIM2_TICS_PER_PERIOD (CLOCK_FREQUENCY / TIM2_FREQ_HZ) 
#define TIM2_MINIMUM_ON_VALUE ( (MINIMUM_ON_TIME_US * 6) )
#define TIM2_MAXIMUM_ON_VALUE ( (MAXIMUM_ON_TIME_US * 6) )

// TODO:  need a way to compute the following.  For now it is OK because the
// PWMs are running at the same frequency.  
#if TIM1_FREQ_HZ > 0
  #define WHITE_TICS_PER_PERIOD  TIM1_TICS_PER_PERIOD

  #define WHITE_MINIMUM_ON_VALUE TIM1_MINIMUM_ON_VALUE
  #define WHITE_MAXIMUM_ON_VALUE TIM1_MAXIMUM_ON_VALUE
#else
  #define WHITE_TICS_PER_PERIOD  TIM2_TICS_PER_PERIOD

  #define WHITE_MINIMUM_ON_VALUE TIM2_MINIMUM_ON_VALUE
  #define WHITE_MAXIMUM_ON_VALUE TIM2_MAXIMUM_ON_VALUE
#endif

#define PWM_POLARITY      EMBER_AF_PLUGIN_PWM_CONTROL_PWM_POLARITY
#define ON_OFF_OUTPUT     EMBER_AF_PLUGIN_PWM_CONTROL_ON_OFF_OUTPUT

static volatile uint32_t *whitePwm, *redPwm, *greenPwm, *bluePwm;
static uint16_t minPwmDrive, maxPwmDrive;

void emberAfPluginBulbPwmConfigurationInitCallback( void )
{
  // limiting choices to PB 1, 2, 3, and 4.  This is Timer 2 alternate
  // channels.  
  emberAfAppPrintln("TICS_PER_PERIOD 2 %d",TIM2_TICS_PER_PERIOD);
  emberAfAppPrintln("MINIMUM_ON_VALUE 2  %d",TIM2_MINIMUM_ON_VALUE);

  TIM2_OR = 
      TIM_REMAPC4 
    | TIM_REMAPC3 
    | TIM_REMAPC2 
    | TIM_REMAPC1;

  TIM2_PSC = 1;      //1^2=2 -> 12MHz/2 = 6 MHz = 6000 ticks per 1/1000 of a second
  TIM2_EGR = 1;      //trigger update event to load new prescaler value
  TIM2_CCMR1  = 0;   //start from a zeroed configuration
  TIM2_ARR = emberAfBulbConfigTicsPerPeriod();  // set the period
  TIM2_CNT = 0; //force the counter back to zero to prevent missing LEVEL_CONTROL_TOP

  // set all PWMs to 0
  TIM2_CCR1 = 0;
  TIM2_CCR2 = 0;
  TIM2_CCR3 = 0; 
  TIM2_CCR4 = 0;

  // Output waveforms on all channels.
  TIM2_CCMR2 |= (0x7 << TIM_OC3M_BIT) |
    (0x7 << TIM_OC4M_BIT);

  TIM2_CCMR1 |= (0x7 << TIM_OC1M_BIT) |
    (0x7 << TIM_OC2M_BIT);

  ATOMIC(
  // polarity:
#if (PWM_POLARITY == 0)
  emberAfAppPrintln("ACTIVE_LOW\r\n");
#if TIM2_FREQ_HZ > 0
  TIM2_CCER |= TIM_CC1P;    // set up PWM 1 as active low
  TIM2_CCER |= TIM_CC2P;    // set up PWM 2 as active low
  TIM2_CCER |= TIM_CC3P;    // set up PWM 3 as active low
  TIM2_CCER |= TIM_CC4P;    // set up PWM 4 as active low
#endif // timer 2 setup

#endif // active low

  TIM2_CCER |= TIM_CC1E;    //enable output on channel 1
  TIM2_CCER |= TIM_CC2E;    //enable output on channel 2
  TIM2_CCER |= TIM_CC3E;    //enable output on channel 3
  TIM2_CCER |= TIM_CC4E;    //enable output on channel 4
  TIM2_CR1  |= TIM_CEN;     //enable counting
  )

  // Set up the PWM register pointers.  Note:  some will be null, depending
  // on how the bulb has been configured.  
  whitePwm = emberAfPwmControlWhitePwm();
  redPwm   = emberAfPwmControlRedPwm();
  greenPwm = emberAfPwmControlGreenPwm();
  bluePwm  = emberAfPwmControlBluePwm();

  minPwmDrive = emberAfPluginBulbConfigMinDriveValue();
  maxPwmDrive = emberAfPluginBulbConfigMaxDriveValue();
}

// limiting the choices to port B pins 1-4.
volatile uint32_t* emChosePwmFromPort( uint8_t port,
                                     uint8_t pin,
                                     volatile uint32_t* defaultPort)
{
  // limit choices to port A for now.
  if(port != EMBER_AF_PLUGIN_BULB_CONFIG_PORT_B) {
    return defaultPort;
  }

  switch(pin) {
  case 1:
    emberSerialPrintf(APP_SERIAL, "PB1\r\n");
    return (volatile uint32_t*) (&TIM2_CCR1);
    break;
  case 2:
    emberSerialPrintf(APP_SERIAL, "PB2\r\n");
    return (volatile uint32_t*) (&TIM2_CCR2);
    break;
  case 3: 
    emberSerialPrintf(APP_SERIAL, "PB3\r\n");
    return (volatile uint32_t*) (&TIM2_CCR3);
    break;
  case 4:
    emberSerialPrintf(APP_SERIAL, "PB4\r\n");
    return (volatile uint32_t*) (&TIM2_CCR4);
    break;
  default:
    emberSerialPrintf(APP_SERIAL, "DEFAULT\r\n");
    return defaultPort;
  }
    
}

volatile uint32_t *emberAfPwmControlWhitePwm( void )
{
  return &PWM_WHITE;
}

volatile uint32_t *emberAfPwmControlRedPwm( void )
{
  return &PWM_RED;
}

volatile uint32_t *emberAfPwmControlGreenPwm( void )
{
  return &PWM_GREEN;
}
volatile uint32_t *emberAfPwmControlBluePwm( void )
{
  return &PWM_BLUE;
}

uint16_t emberAfBulbConfigTicsPerPeriod( void )
{
  uint32_t tics32u;
  uint16_t tics;

  tics32u = CLOCK_FREQUENCY;
  tics32u /= TIM2_FREQ_HZ;

  tics = (uint16_t) tics32u;

  return tics;
}

uint16_t emberAfPluginBulbConfigMinDriveValue( void )
{
  return (uint16_t) (TIM2_MINIMUM_ON_VALUE);
}

uint16_t emberAfPluginBulbConfigMaxDriveValue( void )
{
#ifdef MAXIMUM_ON_TIME_US
  return ((MAXIMUM_ON_TIME_US) * 6);
#else
  // don't specify max on uS.  Use 100% duty cycle by default.
  return (uint16_t) emberAfBulbConfigTicsPerPeriod();
#endif
}

void emberAfPluginBulbConfigDriveWRGB( uint16_t white, uint16_t red, uint16_t green, uint16_t blue )
{
  // level check against minimum pwm drive
  if(white < minPwmDrive && white > 0)
    white = minPwmDrive;
  if(red < minPwmDrive && red > 0)
    red = minPwmDrive;
  if(green < minPwmDrive && green > 0)
    green = minPwmDrive;
  if(blue < minPwmDrive && blue > 0)
    blue = minPwmDrive;

  // As these are all configurable, some of the values may be NULL.  This is
  // OK.  We just need to not write to them if they are null.  
  if(whitePwm != NULL) {
    *whitePwm = white;
  }
  if(redPwm != NULL) {
    *redPwm = red;
  }
  if(greenPwm != NULL) {
    *greenPwm = green;
  }
  if(bluePwm != NULL) {
    *bluePwm = blue;
  }
}

void emberAfPluginBulbConfigDrivePwm( uint16_t value )
{
  emberAfPluginBulbConfigDriveWRGB(value, 0, 0, 0);
}
