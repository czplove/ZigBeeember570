// *****************************************************************************
// * sb1-gesture-sensor.c
// *
// * Routines for interfacing with an i2c based sb1 gesture sensor.  This
// * plugin will receive an interrupt when the sb1 has detected a new gesture,
// * which will cause it to perform an i2c transaction to query the message
// * received.  It will eventually generate a callback that will contain the
// * message received from the gesture sensor.
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "hal/hal.h"
#include "hal/micro/i2c-driver.h"
#include "hal/micro/sb1-gesture-sensor.h"
//------------------------------------------------------------------------------
// Private plugin macros

// Helper functions to verify macro resolution works.
#define PASTE5(a,b,c,d,e)       a##b##c##d##e
#define EVAL5(a,b,c,d,e)        PASTE4(a,b,c,d,e)
#define PASTE4(a,b,c,d)         a##b##c##d
#define EVAL4(a,b,c,d)          PASTE4(a,b,c,d)
#define PASTE(a,b,c)            a##b##c
#define EVAL3(a,b,c)            PASTE(a,b,c)
#define PASTE2(a,b)             a##b
#define EVAL2(a,b)              PASTE2(a,b)
#define EVAL1(a)                a

// If the user defines the GPIO port, pin, and IRQ number in board.h, make all
// of the other defines for them.
#if defined(SB1_IRQ_GPIO_PORT) && defined(SB1_IRQ_GPIO_PIN) \
    && defined(SB1_IRQ_NUMBER)
#define SB1_IRQ_PIN            EVAL3(PORT,SB1_IRQ_GPIO_PORT,_PIN)##(##EVAL1(SB1_IRQ_GPIO_PIN)##)
#define SB1_IRQ_IN_REG         EVAL3(GPIO_P,SB1_IRQ_GPIO_PORT,IN)
#define SB1_IRQ_ISR            EVAL3(halIrq,SB1_IRQ_NUMBER,Isr)
#define SB1_IRQ_INTCFG         EVAL2(GPIO_INTCFG,SB1_IRQ_NUMBER)
#define SB1_IRQ_INT_EN_BIT     EVAL2(INT_IRQ,SB1_IRQ_NUMBER)
#define SB1_IRQ_FLAG_BIT       EVAL3(INT_IRQ,SB1_IRQ_NUMBER,FLAG)
#define SB1_IRQ_MISS_BIT       EVAL2(INT_MISSIRQ,SB1_IRQ_NUMBER)
//
// only define the IRQ select register if we're working with an IRQ that allows
// for changing GPIO IRQ source (C and D)
//
#if (SB1_IRQ_FLAG_BIT==INT_IRQCFLAG) || (SB1_IRQ_FLAG_BIT==INT_IRQDFLAG)
#define SB1_IRQ_SEL_REG        EVAL3(GPIO_IRQ,SB1_IRQ_NUMBER,SEL)
#endif

#endif

// The following macros are absolutely necessary for the gesture sensor to
// function.  If the user does not define them, assign default values based on
// the ISA-39 reference design, and generate lots of warnings so the user knows
// they need to be assigned.

// SB1_IRQ_GPIO, should be something like PORTB_PIN(0)
#ifndef SB1_IRQ_PIN
  #warning "SB1_IRQ_GPIO macro should be defined to IRQ GPIO"
#endif
// SB1_IRQ_IN_REG, should be something like GPIO_PBIN
#ifndef SB1_IRQ_IN_REG
  #warning "SB1_IRQ_IN_REG macro should be defined to IRQ's GPIO input reg"
#endif
#ifndef SB1_IRQ_ISR
  #warning "SB1_IRQ_ISR should be defined to the hal isr for the IRQ used"
#endif
// SB1_IRQ_INTCFG, should be something like GPIO_INTCFGA
#ifndef SB1_IRQ_INTCFG
  #warning "SB1_IRQ_INTCFG should be defined to IRQ's GPIO interrupt cfg reg"
#endif
// SB1_IRQ_INT_EN_BIT, should be something like INT_IRQA
#ifndef SB1_IRQ_INT_EN_BIT
  #warning "SB1_IRQ_INT_EN_BIT should be defined to IRQ's int enable bit"
#endif
// SB1_IRQ_FLAG_BIT, should be something like INT_IRQAFLAG
#ifndef SB1_IRQ_FLAG_BIT
  #warning "SB1_IRQ_FLAG_BIT should be defined to IRQ's interrupt flag bit"
#endif
// SB1_MISSG_BIT, should be something like INT_MISSIRQA
#ifndef SB1_IRQ_MISS_BIT
  #warning "SB1_IRQ_MISS_BIT should be defined to IRQ's missed int status bit"
#endif

// SB1 datasheet specified macros
#define SB1_I2C_ADDR                   0xF0
#define SB1_GESTURE_MSG_LEN            5
#define SB1_GESTURE_MSG_IDX_GESTURE    3

//------------------------------------------------------------------------------
// Private structure and enum declarations
typedef enum {
  SB1_BOTTOM_TOUCH               = 0x01,
  SB1_TOP_TOUCH                  = 0x02,
  SB1_TOP_HOLD                   = 0x03,
  SB1_BOTTOM_HOLD                = 0x04,
  SB1_EITHER_SWIPE_L             = 0x05,
  SB1_EITHER_SWIPE_R             = 0x06,
  SB1_NONE                       = 0x00,
  SB1_NOT_READY                  = 0xFF,
}SB1Message;

//------------------------------------------------------------------------------
// Plugin events
EmberEventControl emberAfPluginSb1GestureSensorMessageReadyEventControl;

//------------------------------------------------------------------------------
// callbacks that must be implemented
void emberAfPluginSb1GestureSensorGestureReceivedCallback(uint8_t gesture,
                                                           uint8_t buttonNum );

//------------------------------------------------------------------------------
// Forward declaration of plugin CLI functions

//------------------------------------------------------------------------------
// Global variables

//------------------------------------------------------------------------------
// Plugin consumed callback implementations

//******************************************************************************
// Plugin init function
//******************************************************************************
void emberAfPluginSb1GestureSensorInitCallback( void )
{
  // Set up the IRQ for the button using the macros defined in the board.h
  // Start from fresh just in case: disable the IRQ, then clear out any existing
  //   interrupts.
  SB1_IRQ_INTCFG = 0;
  INT_CFGCLR = SB1_IRQ_INT_EN_BIT;
  INT_GPIOFLAG = SB1_IRQ_FLAG_BIT;
  INT_MISS = SB1_IRQ_MISS_BIT;

#if (SB1_IRQ_FLAG_BIT==INT_IRQCFLAG) || (SB1_IRQ_FLAG_BIT==INT_IRQDFLAG)
  // Assign the irq to the proper GPIO, if we're using irqC/D
  SB1_IRQ_SEL_REG = SB1_IRQ_PIN;
#endif

  // Disable digital filtering
  SB1_IRQ_INTCFG  = (0 << GPIO_INTFILT_BIT);

  // Set up the interrupt to trigger on both GPIO edges
  SB1_IRQ_INTCFG |= (3 << GPIO_INTMOD_BIT);
  INT_CFGSET |= SB1_IRQ_INT_EN_BIT;
}

//------------------------------------------------------------------------------
// Plugin event handlers

//******************************************************************************
// Event handler called when IRQ goes active.  This is used to prevent I2C
// transactions from occurring within interrupt context.
//******************************************************************************
void emberAfPluginSb1GestureSensorMessageReadyEventHandler(void)
{
  uint8_t slave_addr;
  uint8_t packet_buffer[SB1_GESTURE_MSG_LEN] = {0};
  uint8_t i2cStatus;
  uint8_t buttonNum;
  SB1Message rawMsg;
  Gesture tGestureRxd;

  //We're guaranteed a separate interrupt if the sb1 has another message for
  //us, so we're ok to only execute this once.
  emberEventControlSetInactive(
    emberAfPluginSb1GestureSensorMessageReadyEventControl);

  //Perform the i2c read
  slave_addr = SB1_I2C_ADDR;
  i2cStatus = halI2cReadBytes(slave_addr, packet_buffer, SB1_GESTURE_MSG_LEN);

  //Verify transaction succeeded
  if(i2cStatus != I2C_DRIVER_ERR_NONE) {
    emberAfCorePrintln("I2C failed with err code: %d", i2cStatus);

    //If a message is still waiting, either because the i2c transaction failed
    //or because the sb1 had more than one message ready, reschedule this
    //event.
    halSb1GestureSensorCheckForMsg();
    return;
  }

  //Extract raw command from packet and parse it into a generic gesture
  rawMsg = (SB1Message)packet_buffer[SB1_GESTURE_MSG_IDX_GESTURE];
  switch(rawMsg) {
  case SB1_TOP_TOUCH:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_TOUCH;
    break;
  case SB1_BOTTOM_TOUCH:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_BOTTOM;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_TOUCH;
    break;
  case SB1_TOP_HOLD:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_HOLD;
    break;
  case SB1_BOTTOM_HOLD:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_BOTTOM;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_HOLD;
    break;
  case SB1_EITHER_SWIPE_L:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_SWIPE_L;
    break;
  case SB1_EITHER_SWIPE_R:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_SWIPE_R;
    break;
  case SB1_NONE:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_ERR;
    break;
  case SB1_NOT_READY:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_ERR;
    break;
  default:
    buttonNum = SB1_GESTURE_SENSOR_SWITCH_TOP;
    tGestureRxd = SB1_GESTURE_SENSOR_GESTURE_ERR;
    break;
  }

  emberSerialPrintf(APP_SERIAL, "Sb1 gesture 0x%02x on button 0x%02x\n",
                    tGestureRxd, buttonNum);

  //Send the parsed gesture up to the gesture received callback
  emberAfPluginSb1GestureSensorGestureReceivedCallback((uint8_t)tGestureRxd,
                                                        buttonNum);

  //If a message is still waiting, either because the i2c transaction failed
  //or because the sb1 had more than one message ready, reschedule this
  //event.
  halSb1GestureSensorCheckForMsg();
}

//------------------------------------------------------------------------------
// Plugin public API function implementations

uint8_t halSb1GestureSensorMsgReady(void)
{
#if defined(SB1_IRQ_IN_REG) && defined(SB1_IRQ_PIN)
  //clear int before read to avoid potential of missing interrupt
  if(SB1_IRQ_IN_REG & BIT(SB1_IRQ_PIN & 7)) {
    return(false);
  } else {
    return(true);
  }
#else
  return(false);
#endif
}

uint8_t halSb1GestureSensorCheckForMsg(void)
{
  if(halSb1GestureSensorMsgReady()) {
    emberEventControlSetActive(
      emberAfPluginSb1GestureSensorMessageReadyEventControl);
    return(true);
  }
  return(false);
}

//------------------------------------------------------------------------------
// Plugin CLI function implementations

//******************************************************************************
// Cli command to print the state of the IRQ: message ready or no message rdy
//******************************************************************************
void emAfPluginSb1MessageReady( void )
{
#if defined(SB1_IRQ_IN_REG) && defined(SB1_IRQ_PIN)
  //clear int before read to avoid potential of missing interrupt
  if(SB1_IRQ_IN_REG & BIT(SB1_IRQ_PIN & 7)) {
    emberSerialPrintf(APP_SERIAL, "No msg rdy\n");
    //interrupt is not active
  } else {
    emberSerialPrintf(APP_SERIAL, "msg rdy\n");
    //interrupt is active (active low)
  }
#else
  emberSerialPrintf(APP_SERIAL, "ERR: no IRQpin defined for gesture sensor!\n");
#endif
}

//******************************************************************************
// CLI command to read a single I2C message.  Good for getting raw trace or
// toggling an unserviced IRQ that didn't get picked up by the int handler
//******************************************************************************
void emAfPluginSb1ReadMessage( void )
{
  uint8_t slave_addr;
  uint8_t data[SB1_GESTURE_MSG_LEN] = {0};
  uint8_t i2cStatus;
  uint8_t i;

  //If I2C interrupt is triggered, parse command
  slave_addr = SB1_I2C_ADDR;
  i2cStatus = halI2cReadBytes(slave_addr, data, SB1_GESTURE_MSG_LEN);
  if(i2cStatus == I2C_DRIVER_ERR_NONE) {
    emberSerialPrintf(APP_SERIAL, "Read ");
    for(i=0;i<SB1_GESTURE_MSG_LEN;i++) {
      emberSerialPrintf(APP_SERIAL, "0x%02x ", data[i]);
    }
    emberSerialPrintf(APP_SERIAL, "\n");
  } else {
    emberSerialPrintf(APP_SERIAL, "I2C failed with code: 0x%02x\n", i2cStatus);
  }
}

//******************************************************************************
// plugin sb1 send-gest <gesture> <button>
// This will simulate a gesture being recognized on a button and cause a
// gesture received callback with matching parameters to be generated.  This
// function will perform no sanity checking to verify that the gesture or
// button are sane and exist on the attached sb1 gesture recognition sensor.
//******************************************************************************
void emAfPluginSb1SendGesture(void)
{
  uint8_t gesture = (uint8_t)emberUnsignedCommandArgument(0);
  uint8_t button = (uint8_t)emberUnsignedCommandArgument(1);
  emberAfPluginSb1GestureSensorGestureReceivedCallback(gesture, button);
}

//------------------------------------------------------------------------------
// Plugin ISR function implementations
// WARNING: these functions are in ISR so we must do minimal
// processing and not make any blocking calls (like printf)
// or calls that take a long time.

//******************************************************************************
// Interrupt service routine for IRQ pin.  This function clears the interrupt
// flag and activates the event to handle the message the sb1 has waiting on
// the i2c bus.
//******************************************************************************
#ifdef SB1_IRQ_ISR
void SB1_IRQ_ISR(void)
{
  // ISR CONTEXT!!!
  INT_MISS = SB1_IRQ_MISS_BIT;     //clear missed interrupt flag
  INT_GPIOFLAG = SB1_IRQ_FLAG_BIT; //clear top level interrupt flag

  //
  // Interrupt is active low, so read the GPIO input register for the IRQ pin,
  // then pass that into the BIT macro with the GPIO pin number, which will
  // return 0 if the GPIO is low, or 1 << PIN_NUMBER if the GPIO is high
  //
  if((SB1_IRQ_IN_REG & BIT(SB1_IRQ_PIN & 7)) == 0) {
    emberEventControlSetActive(emberAfPluginSb1GestureSensorMessageReadyEventControl);
  }
}
#endif
