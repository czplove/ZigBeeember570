/***************************************************************************//**
 * @file trace.c
 * @brief TRACE module
 * @version INTERNAL
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
#include "trace.h"
#include "em_device.h"
#include "em_gpio.h"
#include "em_int.h"

/*******************************************************************************
 ********************************   MACROS   ***********************************
 ******************************************************************************/

/* If flag is set in packet header timestamp is not present in the packet. */
#define TRACE_TIMESTAMPPRESENT_FLAG 0x00000080

/* If flag is set in packet header parameter is not present in the packet. */
#define TRACE_NUMOFPARAMETERS_MASK 0x00000060
#define TRACE_NUMOFPARAMETERS_SHIFT 5
#define TRACE_NUMOFPARAMETERS(num) ((num << TRACE_NUMOFPARAMETERS_SHIFT)&TRACE_NUMOFPARAMETERS_MASK)

/*******************************************************************************
 *******************************   STATICS   ***********************************
 ******************************************************************************/
/* Variable storing the mask of enabled ports. */
#if (TRACE_ENABLED == true)
static uint32_t portEnabled;
static uint32_t TRACE_Buffer[TRACE_BUFFER_SIZE];
static uint16_t  rIdx;
static uint16_t  wIdx;
static bool taskMode;
TRACE_SetEventFuncPtr_t pTaskEvtFunc;
static inline void TRACE_QueuePut(uint32_t val);
static uint8_t TRACE_Send(uint32_t * pData, uint8_t length32);
static inline void TRACE_SWOSend(uint32_t val);
static inline bool TRACE_SWOTrySend(uint32_t val);
static void TRACE_Queue0(uint32_t port, char const * str);
static void TRACE_Queue1(uint32_t port, char const * str, uint32_t val);
static void TRACE_Queue2(uint32_t port, char const * str, uint32_t val0, uint32_t val1);
static void TRACE_Queue3(uint32_t port, char const * str, uint32_t val0, uint32_t val1, uint32_t val2);
static inline void TRACE_HeaderInQueuePut(uint32_t port, char const * str, uint8_t numOfArgs);
static inline void TRACE_HeaderDirectSend(uint32_t port, char const * str, uint8_t numOfArgs);
#if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
static inline uint32_t TRACE_TimestampGet(void);
#endif
#endif


/***************************************************************************//**
 * @brief
 *   Initialize TRACE module.
 *
 * @details
 *   Initialize SWO which is later on used for sending trace packets. TRACE
 *   module can be used to log strings and strings with one 32bit parameter.
 *   Logs can be dispatch to one of 32 available ports which can be
 *   independently enabled. Because not all string is sent but only address
 *   of the string logging is very efficient and there is no perfomance cost
 *   in most of the cases.
 *
 ******************************************************************************/
void TRACE_Init(TRACE_Config_t const * config)
{
#if (TRACE_ENABLED == true)
  uint32_t *dwt_ctrl = (uint32_t *) 0xE0001000;
  uint32_t *tpiu_prescaler = (uint32_t *) 0xE0040010;
  uint32_t *tpiu_protocol = (uint32_t *) 0xE00400F0;
  uint32_t *tpiu_ffcr = (uint32_t *) 0xE0040304;

#ifdef EMDRV_TRACE_DEBUGPIN
  /* temporary debug pin */
  GPIO_PinModeSet(EMDRV_TRACE_DEBUGOUT_PORT, EMDRV_TRACE_DEBUGOUT_PIN, gpioModePushPull, 0);
#endif

#ifndef _EFR_DEVICE
  CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_GPIO;
#endif

  /* Enable Serial wire output pin */
  GPIO->ROUTE |= GPIO_ROUTE_SWOPEN;

#if defined(_EFM32_GECKO_FAMILY) || defined(_EFM32_TINY_FAMILY)
  /* Set location 1 */
  GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC1;
  /* Enable output on pin */
  GPIO->P[2].MODEH &= ~(_GPIO_P_MODEH_MODE15_MASK);
  GPIO->P[2].MODEH |= GPIO_P_MODEH_MODE15_PUSHPULL;
#elif defined(_EFM32_GIANT_FAMILY)
  /* Set location 0 */
  GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC0;
  /*Enable output on pin*/
  GPIO->P[5].MODEL &= ~(_GPIO_P_MODEL_MODE2_MASK);
  GPIO->P[5].MODEL |= GPIO_P_MODEL_MODE2_PUSHPULL;
#elif defined(_EFR_DEVICE)
  GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC1;

  /* Enable output on pin */
  GPIO->P[1].MODEH &= ~(_GPIO_P_MODEH_MODE13_MASK);
  GPIO->P[1].MODEH |= GPIO_P_MODEH_MODE13_PUSHPULL;
#else
  #error Unknown device family!
#endif
  /* Enable debug clock AUXHFRCO */
  CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCOEN;

  while(!(CMU->STATUS & CMU_STATUS_AUXHFRCORDY));

  /* Enable trace in core debug */
  CoreDebug->DHCSR |= 1;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  /* Enable PC and IRQ sampling output */
  *dwt_ctrl = 0x400113FF;

#if defined(EFR4DFPGA)
  /* Set TPIU prescaler to 7 (8 MHz / 7 = 1142.857 kHz SWO speed) */
  *tpiu_prescaler = 17;
#else
  /* Set TPIU prescaler to 16 (14 MHz / 16 = 875 kHz SWO speed) */
  *tpiu_prescaler = 0xf;
#endif

  /* Set protocol to NRZ */
  *tpiu_protocol = 2;
  *tpiu_ffcr = 0x100;

  /* Unlock ITM and output data */
  ITM->LAR = 0xC5ACCE55;
  ITM->TCR = 0x10009;

  /* ITM Channel 0 is used for UART output */
  ITM->TER |= 0xF ;

  /* By default enable all debug ports */
  TRACE_PortMaskSet(0xFFFFFFFF);

  if (config->setEventFuncPtr)
  {
    taskMode = true;
    wIdx = 0;
    rIdx = 0;
    pTaskEvtFunc = config->setEventFuncPtr;
  }
  else
  {
    taskMode = false;
  }
#endif
}


/***************************************************************************//**
 * @brief
 *   Configures which ports are enabled.
 *
 * @details
 *   Module support up to 32 ports.
 *
 ******************************************************************************/
void TRACE_PortMaskSet(uint32_t mask)
{
#if (TRACE_ENABLED == true)
  portEnabled = mask;
#endif
}

/***************************************************************************//**
 * @brief
 *   Functions send debug log on given port with 0 to 3 32bit parameters.
 *
 * @param [in] port
 *  Port id (0-31).
 *
 * @param [in] str
 *  Pointer to the string.
 *
 * @param [in] valx
 *  String parameters [0-3].
 *
 * @note
 *  Function checks whether SWO FIFO is busy if so timestamp is not sent since
 *  it means that previous log was just called.
 *
 * @details
 *  Function sends a packet over SWO. Packet consists of:
 *  - Port number + timestampFlag + Parameter present flag(unset): 8bits
 *  - String address: 24bits
 *  - Timestamp: 32bits (optional)
 *  - 0 to 3 Parameters: 32bits
 *
 ******************************************************************************/
void TRACE_Print0(uint32_t port, char const * str)
{
#if (TRACE_ENABLED == true)
  if( port & portEnabled)
  {
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
    if (taskMode)
    {
      TRACE_Queue0(port,str);
    }
    else
    {
      INT_Disable();
      TRACE_HeaderDirectSend(port,str,0);
      INT_Enable();

    }
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
  }
#endif
}
void TRACE_Print1(uint32_t port, char const * str, uint32_t val0)
{
#if (TRACE_ENABLED == true)
  if( port & portEnabled)
  {
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
    if (taskMode)
    {
      TRACE_Queue1(port,str,val0);
    }
    else
    {
      INT_Disable();
      TRACE_HeaderDirectSend(port,str,1);
      TRACE_SWOSend(val0);
      INT_Enable();

    }
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
  }
#endif
}
void TRACE_Print2(uint32_t port, char const * str, uint32_t val0, uint32_t val1)
{
#if (TRACE_ENABLED == true)
  if( port & portEnabled)
  {
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
    if (taskMode)
    {
      TRACE_Queue2(port,str,val0,val1);
    }
    else
    {
      INT_Disable();
      TRACE_HeaderDirectSend(port,str,2);
      TRACE_SWOSend(val0);
      TRACE_SWOSend(val1);
      INT_Enable();

    }
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
  }
#endif
}
void TRACE_Print3(uint32_t port, char const * str, uint32_t val0, uint32_t val1, uint32_t val2)
{
#if (TRACE_ENABLED == true)
  if( port & portEnabled)
  {
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
    if (taskMode)
    {
      TRACE_Queue3(port,str,val0,val1,val2);
    }
    else
    {
      INT_Disable();
      TRACE_HeaderDirectSend(port,str,2);
      TRACE_SWOSend(val0);
      TRACE_SWOSend(val1);
      TRACE_SWOSend(val2);
      INT_Enable();

    }
#ifdef EMDRV_TRACE_DEBUGPIN
    GPIO->P[EMDRV_TRACE_DEBUGOUT_PORT].DOUTTGL = 1 << EMDRV_TRACE_DEBUGOUT_PIN;
#endif
  }
#endif
}

/***************************************************************************//**
 * @brief
 *   Function sends the queued data to SWO. Depending on forceAll flag setting
 *   it terminates once SWO FIFO get full or queue is empty. This function
 *   shall be called by TRACE task.
 *
 * @param [in] forceAll
 *  True - function empties the queue (it stalls once SWO FIFO is busy)
 *  False - function puts data in SWO FIFO until FIFO is full. If not all
 *  data was sent function sends an event to trace task.
 *
 ******************************************************************************/
void TRACE_Flush(bool forceAll)
{
#if (TRACE_ENABLED == true)
  if (rIdx==wIdx)
  {
    return;
  }

  while (forceAll || TRACE_SWOTrySend(TRACE_Buffer[rIdx]))
  {
    rIdx++;
    rIdx &= TRACE_BUFFER_WRAPMASK;
    if (rIdx == wIdx)
    {
      break;
    }
  }
  if (rIdx != wIdx)
  {
    pTaskEvtFunc();
  }
#endif
}

#if (TRACE_ENABLED == true)
/***************************************************************************//**
 * @brief
 *   Function tries to send 32bit words to SWO as long as FIFO is not full.
 *
 * @param [in] val
 *   32bit word.
 *
 * @return [in]
 *  true - word was sent
 *  false - word was not sent
 *
 * @note
 *  Function checks whether SWO FIFO is busy and terminates once it's becomes
 *  busy.
 *
 ******************************************************************************/
static inline bool TRACE_SWOTrySend(uint32_t val)
{
  bool status = false;
  if (ITM->PORT[TRACE_SWO_PORT].u32 != 0)
  {
    ITM->PORT[TRACE_SWO_PORT].u32 = val;
    status = true;
  }
  return status;
}
/***************************************************************************//**
 * @brief
 *   Function sends 32bit words to SWO.
 *
 * @param [in] val
 *   32bit word.
 *
 * @note
 *  Function pends if SWO FIFO is not empty
 *
 ******************************************************************************/
static inline void TRACE_SWOSend(uint32_t val)
{
  while (TRACE_SWOTrySend(val)==false);
}
/***************************************************************************//**
 * @brief
 *   Function adds single packet header to the buffer queue. Header is a
 *   32 bit word which contains string address, timestamp presence flag and
 *   number of arguments indicator.
 *
 * @param [in] port
 *  Trace port number [0-31]
 *
 * @param [in] str
 *  String address.
 *
 * @param [in] numOfArgs
 *  Number of arguments in the log packet.
 *
 ******************************************************************************/
static inline void TRACE_HeaderInQueuePut(uint32_t port, char const * str, uint8_t numOfArgs)
{
  port = __CLZ(__RBIT(port));
  #if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
    port |= (TRACE_TIMESTAMPPRESENT_FLAG | TRACE_NUMOFPARAMETERS(numOfArgs));
  #else
    port |= TRACE_NUMOFPARAMETERS(numOfArgs);
  #endif

    TRACE_QueuePut((uint32_t)str | (port<<24));
  #if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
    TRACE_QueuePut(TRACE_TimestampGet());
  #endif
}
/***************************************************************************//**
 * @brief
 *   Function sends packet header over SWO. Header is a 32 bit word which
 *   contains string address, timestamp presence flag and number of arguments
 *   indicator.
 *
 * @param [in] port
 *  Trace port number [0-31]
 *
 * @param [in] str
 *  String address.
 *
 * @param [in] numOfArgs
 *  Number of arguments in the log packet.
 *
 ******************************************************************************/
static inline void TRACE_HeaderDirectSend(uint32_t port, char const * str, uint8_t numOfArgs)
{
  bool useTimestamp = true;
  port = __CLZ(__RBIT(port));
  port |= TRACE_TIMESTAMPPRESENT_FLAG | TRACE_NUMOFPARAMETERS(numOfArgs);

#if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
  if( ITM->PORT[TRACE_SWO_PORT].u32 == 0 )
#endif
  {
    useTimestamp = false;
    port &= ~TRACE_TIMESTAMPPRESENT_FLAG;
  }

  TRACE_SWOSend((uint32_t)str | (port<<24));
  if(useTimestamp)
  {
#if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
    TRACE_SWOSend(TRACE_TimestampGet());
#endif
  }
}
/***************************************************************************//**
 * @brief
 *   Function adds log (string with value) to the queue to be sent to SWO. There
 *   are 4 functions available: 0 string arguments,1,2 and 3 32 bit arguments.
 *
 * @param [in] port
 *  Trace port number [0-31]
 *
 * @param [in] str
 *  String address.
 *
 * @param [in] valx
 *  Value used as string parameter.
 *
 ******************************************************************************/
static void TRACE_Queue0(uint32_t port, char const * str)
{
  INT_Disable();

  TRACE_HeaderInQueuePut(port,str,0);
  pTaskEvtFunc();

  INT_Enable();
}
static void TRACE_Queue1(uint32_t port, char const * str, uint32_t val)
{
  INT_Disable();

  TRACE_HeaderInQueuePut(port,str,1);
  TRACE_QueuePut(val);

  pTaskEvtFunc();

  INT_Enable();
}
static void TRACE_Queue2(uint32_t port, char const * str, uint32_t val0, uint32_t val1)
{
  INT_Disable();

  TRACE_HeaderInQueuePut(port,str,2);
  TRACE_QueuePut(val0);
  TRACE_QueuePut(val1);

  pTaskEvtFunc();

  INT_Enable();
}
static void TRACE_Queue3(uint32_t port, char const * str, uint32_t val0, uint32_t val1, uint32_t val2)
{
  INT_Disable();

  TRACE_HeaderInQueuePut(port,str,3);
  TRACE_QueuePut(val0);
  TRACE_QueuePut(val1);
  TRACE_QueuePut(val2);

  pTaskEvtFunc();

  INT_Enable();
}
/***************************************************************************//**
 * @brief
 *   Function returns 32 bit long timestamp.
 *
 * @return timestamp
 *
 ******************************************************************************/
#if (TRACE_TIMESTAMP_MODE != TRACE_TIMESTAMP_DISABLED)
static inline uint32_t TRACE_TimestampGet(void)
{
#if (TRACE_TIMESTAMP_MODE==TRACE_TIMESTAMP_RTCCPROTIMER)
  uint32_t wrapcnt = PROTIMER->WRAPCNT & 0x0000FFFF;
  return (RTCC->CNT <<16) | wrapcnt;
#elif (TRACE_TIMESTAMP_MODE==TRACE_TIMESTAMP_CYCCNT)
  return DWT->CYCCNT;
#endif
}
#endif
/***************************************************************************//**
 * @brief
 *   Function adds word to TRACE queue.
 *
 ******************************************************************************/
static inline void TRACE_QueuePut(uint32_t val)
{
  TRACE_Buffer[wIdx++] = val;
  wIdx &= TRACE_BUFFER_WRAPMASK;
}
#endif

/** @} (end addtogroup EFRDRV) */
/** @} (end addtogroup EM_Library) */
