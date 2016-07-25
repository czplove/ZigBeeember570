/***************************************************************************//**
 * @file trace.h
 * @brief EMDRV_Trace - tracing module utilizing SWO
 * @version 0.0.9-INTERNAL
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
#ifndef __EMDRV_TRACE_H
#define __EMDRV_TRACE_H

#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup EM_Library
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup TRACE
 * @{
 ******************************************************************************/

/*******************************************************************************
 ********************************   MACROS   ***********************************
 ******************************************************************************/
#ifdef TRACE_CONFIG_FILE
  #include "trace_config.h"
#endif

#ifndef TRACE_ENABLED
  #define TRACE_ENABLED false
#endif

#ifndef TRACE_BUFFER_POW2SIZE
  #define TRACE_BUFFER_POW2SIZE 8
#endif

#ifndef TRACE_SWO_PORT
  #define TRACE_SWO_PORT 0
#endif

#if TRACE_BUFFER_POW2SIZE > 16
#error "Trace buffer too big. Maximum 2^16*4b=256kB"
#endif

#define TRACE_BUFFER_SIZE     (1<<TRACE_BUFFER_POW2SIZE)
#define TRACE_BUFFER_WRAPMASK (TRACE_BUFFER_SIZE-1)

#define TRACE_TIMESTAMP_DISABLED 0
#define TRACE_TIMESTAMP_CYCCNT 1
#define TRACE_TIMESTAMP_RTCCPROTIMER 2

#ifdef TRACE_TIMESTAMP_MODE_DISABLED
  #define TRACE_TIMESTAMP_MODE TRACE_TIMESTAMP_DISABLED
#elif defined TRACE_TIMESTAMP_MODE_CYCCNT
  #define TRACE_TIMESTAMP_MODE TRACE_TIMESTAMP_CYCCNT
#elif defined TRACE_TIMESTAMP_MODE_RTCCPROTIMER
  #define TRACE_TIMESTAMP_MODE TRACE_TIMESTAMP_RTCCPROTIMER
#else
  #define TRACE_TIMESTAMP_MODE TRACE_TIMESTAMP_DISABLED
#endif

#define TRACE_PORT_PHY          0x00000001
#define TRACE_PORT_PHY_DATA     0x00000002
#define TRACE_PORT_SAP          0x00000004
#define TRACE_PORT_SERINS       0x00000004
#define TRACE_PORT_SEQ          0x00000008
#define TRACE_PORT_MLME         0x00000200
#define TRACE_PORT_MACMAN       0x00000008
#define TRACE_PORT_INDIRTX      0x00000400
#define TRACE_PORT_MCPS         0x00000010
#define TRACE_PORT_BSCH         0x00000020
#define TRACE_PORT_CUSTOM       0x00000040
#define TRACE_PORT_RADIODRV     0x00000080
#define TRACE_PORT_ASSERT       0x00000100

#ifdef EMDRV_TRACE_DEBUGPIN
#define EMDRV_TRACE_DEBUGOUT_PORT gpioPortA
#define EMDRV_TRACE_DEBUGOUT_PIN  1
#endif

/*******************************************************************************
 ******************************   PROTOTYPES   *********************************
 ******************************************************************************/
typedef void (*TRACE_SetEventFuncPtr_t)(void);

typedef struct
{
  TRACE_SetEventFuncPtr_t setEventFuncPtr;
} TRACE_Config_t;

void TRACE_Init(TRACE_Config_t const * config);
void TRACE_Flush(bool forceAll);

void TRACE_PortMaskSet(uint32_t mask);
void TRACE_Print0(uint32_t port, char const * str);
void TRACE_Print1(uint32_t port, char const * str, uint32_t val0);
void TRACE_Print2(uint32_t port, char const * str, uint32_t val0, uint32_t val1);
void TRACE_Print3(uint32_t port, char const * str, uint32_t val0, uint32_t val1, uint32_t val2);

#if (TRACE_ENABLED == true)
  #define TRACE_LogShort(port, str)            TRACE_Print0(port,str)
  #define TRACE_Log(port, str,val)             TRACE_Print1(port,str,val)
  #define TRACE_Log0(port, str)                TRACE_Print0(port,str)
  #define TRACE_Log1(port, str,val0)           TRACE_Print1(port,str,val0)
  #define TRACE_Log2(port, str,val0,val1)      TRACE_Print2(port,str,val0,val1)
  #define TRACE_Log3(port, str,val0,val1,val2) TRACE_Print3(port,str,val0,val1,val2)
#else
  #define TRACE_LogShort(port, str)
  #define TRACE_Log(port, str,val)

  #define TRACE_Log0(port, str)
  #define TRACE_Log1(port, str,val0)
  #define TRACE_Log2(port, str,val0,val1)
  #define TRACE_Log3(port, str,val0,val1,val2)
#endif

/** @} (end addtogroup TRACE) */
/** @} (end addtogroup EM_Library) */

#ifdef __cplusplus
}
#endif


#endif /* __EMDRV_TRACE_H */
