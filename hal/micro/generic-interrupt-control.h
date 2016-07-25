// *****************************************************************************
// * Generic Interrupt Control.h
// *
// * Plugin used to allow user to implement various interrupt handling
// * functionality in a hardware abstracted way
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

#ifndef __GENERIC_INTERRUPT_CONTROL_H__
#define __GENERIC_INTERRUPT_CONTROL_H__

//------------------------------------------------------------------------------
// Plugin public macro definitions

#define HAL_GIC_INT_CFG_EDGE_BOTH     3
#define HAL_GIC_INT_CFG_EDGE_POS      1
#define HAL_GIC_INT_CFG_EDGE_NEG      2

#define HAL_GIC_GPIO_PORTA            1
#define HAL_GIC_GPIO_PORTB            2
#define HAL_GIC_GPIO_PORTC            3
#define HAL_GIC_GPIO_PORTD            4

#define HAL_GIC_IRQ_NUMA              1
#define HAL_GIC_IRQ_NUMB              2
#define HAL_GIC_IRQ_NUMC              3
#define HAL_GIC_IRQ_NUMD              4

//------------------------------------------------------------------------------
// Plugin public struct and enum definitions
typedef struct tIrqCfg
{
  uint32_t irqPin;
  uint32_t *irqInReg;
  uint32_t *irqIntCfgReg;
  uint32_t irqIntEnBit;
  uint32_t irqFlagBit;
  uint32_t irqMissBit;
  uint32_t irqEdgeCfg;
  uint32_t irqSelBit;
  //fxn pointer for non-default ISR
  void (*irqISR)(void); //fxn pointer for non-default ISR
  EmberEventControl *irqEventHandler; //ember event for default delayed handling
} HalGenericInterruptControlIrqCfg;

//------------------------------------------------------------------------------
// Plugin public functions

//------------------------------------------------------------------------------
// Initialize the specified irq configuration structure.
//
// This function will take as input the GPIO pin (0-7) and port (A-D) and the
// IRQ to be associated with that pin (A-D).  It will configure, but not
// enable, the specified IRQ and return a pointer to the IRQ structure that
// should be used as the primary parameter of all other functions defined in
// this plugin.
//
// @param uint8_t irqPin: This defines the GPIO pin number to be associated with
//        the interrupt.  It should be a literal number, 0-7.
// @param uint8_t irqPort: This defines the GPIO port letter to be associated with
//        the interrupt.  It should be one of the following macros:
//        GIC_GPIO_PORTA, GIC_GPIO_PORTB, GIC_GPIO_PORTC, or GIC_GPIO_PORTD
// @param uint8_t irqNum: This defines the irq to be assigned to the specified
//        GPIO port and pin.  It should be one of the following macros:
//        GIC_IRQ_NUMA, GIC_IRQ_NUMB, GIC_IRQ_NUMC, or GIC_IRQ_NUMD
//
// @return This function will return a pointer to the structure that will be
//         permanently attached to the provided IRQ.  That structure should
//         be used as the input parameter to all other public functions defined
//         in this API.  
HalGenericInterruptControlIrqCfg* halGenericInterruptControlIrqCfgInitialize(
  uint8_t irqPin,
  uint8_t irqPort,
  uint8_t irqNum);
  
//------------------------------------------------------------------------------
// Configure the edges on which the HW ISR will execute
// This function will set up the GPIO hardware to interrupt on the conditions
// specified as the edge parameter.  It will modify the configuration structure
// and set up the GPIO peripheral.
//
// @param HalGenericInterruptControlIrqCfg *config is the configure structure of the IRQ to be changed.
// @param uint8_t edge is the new edge configuration for the interrupt.  It can
//        be set to any of the following: GIC_INT_CFG_EDGE_BOTH,
//        GIC_INT_CFG_EDGE_POS, or GIC_INT_CFG_EDGE_NEG
void halGenericInterruptControlIrqEdgeConfig(
  HalGenericInterruptControlIrqCfg *config,
  uint8_t edge);
  
//------------------------------------------------------------------------------
// Assign an ISR to the IRQ
// This function will specify a function to call at interrupt context for the
// IRQ.  If this function is specified, no work will be done by the default GIC
// ISR, so the user will have to implement any interrupt clearing functionality
// necessary for the IRQ to behave as the user intends.  If an event has been
// registered with the IRQ via the @gicIRQEventRegister function, it will still
// be activated by the GIC's implemented ISR after the user specified ISR has
// executed.
//
// @param HalGenericInterruptControlIrqCfg *config is the configuration structure of the IRQ to be changed
// @param void (Iisr)(void) is a function pointer to the ISR that should be
//        executed in interrupt context when the HW interrupt occurs.
void halGenericInterruptControlIrqIsrAssignFxn(
  HalGenericInterruptControlIrqCfg *config,
  void (*isr)(void));
  
//------------------------------------------------------------------------------
// Removes an ISR from the IRQ
// This function will cause the user specified ISR to no longer take place.  If
// an interrupt occurs, the GIC's default ISR will execute instead of a user
// defined ISR
void halGenericInterruptControlIrqIsrRemoveFxn(
  HalGenericInterruptControlIrqCfg *config);
  
//------------------------------------------------------------------------------
// Register an event to take place when an interrupt occurs
// This function will set what event (if any) should be activated when an
// interrupt occurs.  The scheduling of this event will take place while still
// in interrupt context, but the user can be guaranteed that the processor will
// no longer be within interrupt context by the time the event executes.
//
// @param HalGenericInterruptControlIrqCfg *config is the configuration structure of the IRQ to be changed
// @param EmberEventControl *event is a pointer to the event to be activated
//        when on interrupt
void halGenericInterruptControlIrqEventRegister(
  HalGenericInterruptControlIrqCfg *config,
  EmberEventControl *event);
  
//------------------------------------------------------------------------------
// Enable the IRQ
// This function will enable the IRQ associated with the given config struct
//
// @param HalGenericInterruptControlIrqCfg *config is the configuration
//        structure of the IRQ to be enabled
void halGenericInterruptControlIrqEnable(
  HalGenericInterruptControlIrqCfg *config);
  
//------------------------------------------------------------------------------
// Disable the IRQ
// This function disable the interrupt specified by the input parameter.
//
// @param HalGenericInterruptControlIrqCfg *config is the config structure of
//        the IRQ to be disaabled
void halGenericInterruptControlIrqDisable(
  HalGenericInterruptControlIrqCfg *config);
  
//------------------------------------------------------------------------------
// Read the IRQ GPIO
// This function will read the GPIO pin that has been assigned to the IRQ
// specified by the input parameter.
//
// @param HalGenericInterruptControlIrqCfg *config is the config structure of the IRQ whose GPIO pin is to
//        be read.
// @return This function will return 0 if the GPIO pin is at a logic low, or
//         non-zero if the GPIO pin is at a logic high.
uint8_t halGenericInterruptControlIrqReadGpio(
  HalGenericInterruptControlIrqCfg *config);

#endif //__GENERIC_INTERRUPT_CONTROL_H__

