/***************************************************************************//**
 * @file com.c
 * @brief COM Layer.
 * @version 0.01.0
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2014 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *

 ******************************************************************************/

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"

#include "hal/hal.h"

#include <stdarg.h>

#ifdef CORTEXM3_EFM32_MICRO
#include "em_device.h"
#include "em_int.h"
#include "em_usart.h"
#endif

#include "com.h"
#include "ember-printf.h"

// Static declarations
static void reloadUartBuffer(UARTDRV_Handle_t handle,
                             Ecode_t transferStatus,
                             uint8_t *data,
                             UARTDRV_Count_t transferCount);

/* Initialization data and buffer queues */

#ifdef COM_VCP_ENABLE
  //add VCP support
  #include "stack/include/ember-debug.h"
  #define COM_0_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_0_TX_BUF_QUEUE_ADDR (NULL)
  /* FIFO Buffers */
  DEFINE_FIFO_QUEUE(COM_0_RX_QUEUE_SIZE,com0RxFifoQueue)
  #define COM_0_RX_QUEUE_ADDR (&com0RxFifoQueue)
  DEFINE_FIFO_QUEUE(COM_0_TX_QUEUE_SIZE,com0TxFifoQueue)
  #define COM_0_TX_QUEUE_ADDR (&com0TxFifoQueue)
#else
  #define COM_0_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_0_TX_BUF_QUEUE_ADDR (NULL)
  #define COM_0_RX_QUEUE_ADDR (NULL)
  #define COM_0_TX_QUEUE_ADDR (NULL)
#endif //COM_VCP_ENABLE

#ifdef COM_USART0_ENABLE
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_RX_BUFS, com1RxBufferQueue);
  #define COM_1_RX_BUF_QUEUE_ADDR (&com1RxBufferQueue)
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_TX_BUFS, com1TxBufferQueue);
  #define COM_1_TX_BUF_QUEUE_ADDR (&com1TxBufferQueue)

  /* FIFO Buffers */
  DEFINE_FIFO_QUEUE(COM_1_RX_QUEUE_SIZE,com1RxFifoQueue)
  #define COM_1_RX_QUEUE_ADDR (&com1RxFifoQueue)
  DEFINE_FIFO_QUEUE(COM_1_TX_QUEUE_SIZE,com1TxFifoQueue)
  #define COM_1_TX_QUEUE_ADDR (&com1TxFifoQueue)
#else
  #define COM_1_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_1_TX_BUF_QUEUE_ADDR (NULL)
  #define COM_1_RX_QUEUE_ADDR (NULL)
  #define COM_1_TX_QUEUE_ADDR (NULL)
#endif

#ifdef COM_USART1_ENABLE
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_RX_BUFS, com2RxBufferQueue);
  #define COM_2_RX_BUF_QUEUE_ADDR (&com2RxBufferQueue)
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_TX_BUFS, com2TxBufferQueue);
  #define COM_2_TX_BUF_QUEUE_ADDR (&com2TxBufferQueue)

  /* FIFO Buffers */
  DEFINE_FIFO_QUEUE(COM_2_RX_QUEUE_SIZE,com2RxFifoQueue)
  #define COM_2_RX_QUEUE_ADDR (&com2RxFifoQueue)
  DEFINE_FIFO_QUEUE(COM_2_TX_QUEUE_SIZE,com2TxFifoQueue)
  #define COM_2_TX_QUEUE_ADDR (&com2TxFifoQueue)
#else
  #define COM_2_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_2_TX_BUF_QUEUE_ADDR (NULL)
  #define COM_2_RX_QUEUE_ADDR (NULL)
  #define COM_2_TX_QUEUE_ADDR (NULL)
#endif

#ifdef COM_USART2_ENABLE
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_RX_BUFS, com3RxBufferQueue);
  #define COM_3_RX_BUF_QUEUE_ADDR (&com3RxBufferQueue)
  DEFINE_BUF_QUEUE(EMDRV_UARTDRV_MAX_CONCURRENT_TX_BUFS, com3TxBufferQueue);
  #define COM_3_TX_BUF_QUEUE_ADDR (&com3TxBufferQueue)

  /* FIFO Buffers */
  DEFINE_FIFO_QUEUE(COM_3_RX_QUEUE_SIZE,com3RxFifoQueue)
  #define COM_3_RX_QUEUE_ADDR (&com3RxFifoQueue)
  DEFINE_FIFO_QUEUE(COM_3_TX_QUEUE_SIZE,com3TxFifoQueue)
  #define COM_3_TX_QUEUE_ADDR (&com3TxFifoQueue)
#else
  #define COM_3_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_3_TX_BUF_QUEUE_ADDR (NULL)
  #define COM_3_RX_QUEUE_ADDR (NULL)
  #define COM_3_TX_QUEUE_ADDR (NULL)
#endif

#ifdef COM_USB_ENABLE
  //add USB support

  #define COM_4_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_4_TX_BUF_QUEUE_ADDR (NULL)
  /* FIFO Buffers */
  DEFINE_FIFO_QUEUE(COM_4_RX_QUEUE_SIZE,com4RxFifoQueue)
  #define COM_4_RX_QUEUE_ADDR (&com4RxFifoQueue)
  DEFINE_FIFO_QUEUE(COM_4_TX_QUEUE_SIZE,com4TxFifoQueue)
  #define COM_4_TX_QUEUE_ADDR (&com4TxFifoQueue)
#else
  #define COM_4_RX_BUF_QUEUE_ADDR (NULL)
  #define COM_4_TX_BUF_QUEUE_ADDR (NULL)
  #define COM_4_RX_QUEUE_ADDR (NULL)
  #define COM_4_TX_QUEUE_ADDR (NULL)
#endif

/* Handles */
DEFINE_UART_HANDLES(uarthandle)
DEFINE_COM_HANDLES(comhandle)

void *comTxQueues[COM_PORTS] = 
  { FOR_EACH_PORT( (void *),COM,TX_QUEUE_ADDR ) };
void *comRxQueues[COM_PORTS] = 
  { FOR_EACH_PORT( (void *),COM,RX_QUEUE_ADDR ) };
uint16_t PGM comTxQueueSize[COM_PORTS] = 
  { FOR_EACH_PORT( (uint16_t), COM,TX_QUEUE_SIZE ) };
uint16_t PGM comRxQueueSize[COM_PORTS] = 
  { FOR_EACH_PORT( (uint16_t), COM,RX_QUEUE_SIZE ) };
UARTDRV_Buffer_FifoQueue_t *comRxBufferQueues[COM_PORTS] = 
  { FOR_EACH_PORT( (UARTDRV_Buffer_FifoQueue_t *), COM,RX_BUF_QUEUE_ADDR) };
UARTDRV_Buffer_FifoQueue_t *comTxBufferQueues[COM_PORTS] = 
  { FOR_EACH_PORT( (UARTDRV_Buffer_FifoQueue_t *), COM,TX_BUF_QUEUE_ADDR) };

uint32_t dma_IEN;
#if LDMA_COUNT > 0
  #define DMA_REG LDMA
#elif  DMA_COUNT > 0
  #define DMA_REG DMA
#endif
/* "power down" COM by switching from DMA to UART byte interrupts */
void COM_InternalPowerDown()
{ 
  if (DMA_REG->IEN!=0)
  {
    dma_IEN= DMA_REG->IEN;
    DMA_REG->IEN = 0;
  }
  #ifdef COM_USART0_ENABLE
    NVIC_ClearPendingIRQ( USART0_RX_IRQn );
    NVIC_EnableIRQ( USART0_RX_IRQn );
    USART_IntEnable(USART0, USART_IF_RXDATAV);
  #endif
  #ifdef COM_USART1_ENABLE
    NVIC_ClearPendingIRQ( USART1_RX_IRQn );
    NVIC_EnableIRQ( USART1_RX_IRQn );
    USART_IntEnable(USART1, USART_IF_RXDATAV);
  #endif
  #ifdef COM_USART2_ENABLE
    NVIC_ClearPendingIRQ( USART2_RX_IRQn );
    NVIC_EnableIRQ( USART2_RX_IRQn );
    USART_IntEnable(USART2, USART_IF_RXDATAV);
  #endif
}

/* "power up" COM by switching back to DMA interrupts */
void COM_InternalPowerUp()
{
  #ifdef COM_USART0_ENABLE
    USART_IntClear(USART0, USART_IF_RXDATAV);
    USART_IntDisable(USART0, USART_IF_RXDATAV);
    NVIC_ClearPendingIRQ( USART0_RX_IRQn );
    NVIC_DisableIRQ( USART0_RX_IRQn );
  #endif
  #ifdef COM_USART1_ENABLE
    USART_IntClear(USART1, USART_IF_RXDATAV);
    USART_IntDisable(USART1, USART_IF_RXDATAV);
    NVIC_ClearPendingIRQ( USART1_RX_IRQn );
    NVIC_DisableIRQ( USART1_RX_IRQn );
  #endif
  #ifdef COM_USART2_ENABLE
    USART_IntClear(USART2, USART_IF_RXDATAV);
    USART_IntDisable(USART2, USART_IF_RXDATAV);
    NVIC_ClearPendingIRQ( USART2_RX_IRQn );
    NVIC_DisableIRQ( USART2_RX_IRQn );
  #endif
  if (dma_IEN!=0)
  {
    DMA_REG->IEN = dma_IEN;
  }
}

static inline bool getOutputFifoSpace(COM_Port_t port, uint8_t extraByteCount)
{
  return (comhandle[port]->txQueue->used < comhandle[port]->txsize - extraByteCount);
}

static inline bool checkValidPort(COM_Port_t port)
{
  switch (port) {
  #ifdef COM_VCP_ENABLE
    case COM_VCP:
        return true;
  #endif
  #ifdef COM_USART0_ENABLE
    case COM_USART0:
        return true;
  #endif
  #ifdef COM_USART1_ENABLE
    case COM_USART1:
        return true;
  #endif
  #ifdef COM_USART2_ENABLE
    case COM_USART2:
        return true;
  #endif
  #ifdef COM_USB_ENABLE
    case COM_USB:
        return true;
  #endif
    default:
      return false;
  }
}

bool COM_InternalTxIsIdle(COM_Port_t port)
{
  // UART
  if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
  {
    return (USART_StatusGet(comhandle[port]->uarthandle->initData.port) & _USART_STATUS_TXIDLE_MASK) ? true : false;
  }
  return false;
}

void pumpRx(COM_Port_t port)
{
  if (checkValidPort(port) == false)
  {
    return;
  }
  // UART
  if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
  {
    uint8_t *buf;
    UARTDRV_Count_t xferred, remaining;
    INT_Disable();
    UARTDRV_GetReceiveStatus(comhandle[port]->uarthandle, &buf, &xferred, &remaining);
    updateBuffer(comhandle[port]->rxQueue, xferred, comhandle[port]->rxsize);
    INT_Enable();
  }
#ifdef COM_VCP_ENABLE
  if (port == COM_VCP)
  {
    //VCP functionality (VUART) on EFRs do not yet support Rx.
    return;
    //x uint8_t rxbyte;
    //x uint16_t xferred;
    //x BSP_vcpRead( &rxbyte, 1, 1, &xferred, false );
    //x if (xferred>0)
    //x {
    //x   INT_Disable();
    //x   FIFO_ENQUEUE(comhandle[port]->rxQueue, rxbyte, comhandle[port]->rxsize);
    //x   INT_Enable();
    //x }
  }
#endif
}

/* reload buffer callbacks */
void updateBuffer(COM_FifoQueue_t *q, uint16_t xferred, uint16_t size)
{
  // Update tail with additional xferred. Data should already be there
  INT_Disable();
    if (xferred > q->pumped)
    {
    q->head = ((q->head - q->pumped + xferred) % size);
    q->used += xferred - q->pumped;
    q->pumped = xferred;
    }
  INT_Enable();
}

static void reloadUartBuffer(UARTDRV_Handle_t handle,
                      Ecode_t transferStatus, 
                      uint8_t *data,
                      UARTDRV_Count_t transferCount)
{
  // figure out which com port this is
  for (uint8_t i=0; i<COM_PORTS; i++)
  {
    if (comhandle[i]->uarthandle == handle)
    {
      // update fifo
      updateBuffer(comhandle[i]->rxQueue, transferCount, comhandle[i]->rxsize);
      // reset pumped bytes (already in interrupt context)
      comhandle[i]->rxQueue->pumped = 0;
      // reset receive operation
      UARTDRV_Receive(comhandle[i]->uarthandle,
                      data,
                      comhandle[i]->rxsize/2, 
                      reloadUartBuffer);
    }
  }
}

static void dequeueFifoBuffer(COM_Port_t port, uint16_t count)
{
  INT_Disable();
  FIFO_DEQUEUE_MULTIPLE(comhandle[port]->txQueue,comhandle[port]->txsize,count);
  INT_Enable();
}

static void unloadTxBuffer(UARTDRV_Handle_t handle,
                      Ecode_t transferStatus, 
                      uint8_t *data,
                      UARTDRV_Count_t transferCount)
{
  // figure out which com port this is
  for (uint8_t port=0; port<COM_PORTS; port++)
  {
    if (comhandle[port]->uarthandle == handle)
    {
      dequeueFifoBuffer((COM_Port_t) port, transferCount);
    }
  }
}
//------------------------------------------------------
// Serial initialization

Ecode_t COM_Init(COM_Port_t port, COM_Init_t *init)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }

  comhandle[port]->rxQueue = (COM_FifoQueue_t *) comRxQueues[port];
  comhandle[port]->txQueue = (COM_FifoQueue_t *) comTxQueues[port];
  comhandle[port]->rxsize = comRxQueueSize[port];
  comhandle[port]->txsize = comTxQueueSize[port];
  // VCP

#if (COM_VCP_PORTS > 0) && defined(COM_VCP_ENABLE)
  if (port == COM_VCP)
  {
    //emDebugInit() should have already been called. 
  }
#endif


  // UART
#if (COM_UART_PORTS > 0)
  if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
  {
    Ecode_t status;

    comhandle[port]->uarthandle = uarthandle[port-COM_VCP_PORTS];

    // add rx/tx buffer queue to initdata
    init->uartdrvinit.rxQueue = comRxBufferQueues[port];
    init->uartdrvinit.txQueue = comTxBufferQueues[port];

    // iniitalize hardware
    status = UARTDRV_Init(uarthandle[port-COM_VCP_PORTS], &init->uartdrvinit);
    if (status != EMBER_SUCCESS)
    {
      return status;
    }
    // start ping pong buffers for FIFO
    UARTDRV_Receive(uarthandle[port-COM_VCP_PORTS], 
                    comhandle[port]->rxQueue->fifo, 
                    comhandle[port]->rxsize/2, 
                    reloadUartBuffer);
    UARTDRV_Receive(uarthandle[port-COM_VCP_PORTS],
                    &comhandle[port]->rxQueue->fifo[comhandle[port]->rxsize/2],
                    comhandle[port]->rxsize/2, 
                    reloadUartBuffer);

    #ifndef ENABLE_EXP_UART
      halEnableVCOM();
    #endif
  }
#endif //(COM_UART_PORTS > 0)

  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Serial Input

// returns # bytes available for reading
uint16_t COM_ReadAvailable(COM_Port_t port)  
{
  // make sure rx buffer is updated
  pumpRx(port);
  return comhandle[port]->rxQueue->used;
}

Ecode_t COM_ReadByte(COM_Port_t port, uint8_t *dataByte)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  // make sure rx buffer is updated
  pumpRx(port);
  if (comhandle[port]->rxQueue->used > 0) {
    INT_Disable();
      *dataByte = FIFO_DEQUEUE(comhandle[port]->rxQueue, comhandle[port]->rxsize);
    INT_Enable();
    return EMBER_SUCCESS;
  }
  return EMBER_SERIAL_RX_EMPTY;
}

Ecode_t COM_ReadData(COM_Port_t port,
                                uint8_t *data,
                                uint16_t length,
                                uint16_t *bytesRead)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  uint16_t bytesReadInternal = 0;
  Ecode_t status;

  while (bytesReadInternal < length) {
    status = COM_ReadByte(port, data);

    switch (status) {
      case EMBER_SUCCESS:
        ++data;
        ++bytesReadInternal;
        break;

      case EMBER_SERIAL_RX_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead) {
    *bytesRead = bytesReadInternal;
  }

  return EMBER_SUCCESS;

}

#ifndef EMBER_TEST
Ecode_t COM_ReadDataTimeout(COM_Port_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead,
                                       uint16_t firstByteTimeout,
                                       uint16_t subsequentByteTimeout)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  uint16_t bytesReadInternal = 0;
  Ecode_t status;
  uint16_t timeout = firstByteTimeout;
  uint16_t startTime = halCommonGetInt16uMillisecondTick();
  
  // loop until we read the max number of bytes or the timeout elapses
  while (bytesReadInternal < length
         && elapsedTimeInt16u(startTime,halCommonGetInt16uMillisecondTick()) < timeout) {
    status = COM_ReadByte(port, data);

    switch (status) {
      case EMBER_SUCCESS:
        ++data;
        ++bytesReadInternal;
        // reset timer and timeout for next character
        startTime = halCommonGetInt16uMillisecondTick();
        timeout = subsequentByteTimeout;
        break;

      case EMBER_SERIAL_RX_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead) {
    *bytesRead = bytesReadInternal;
  }

  return bytesReadInternal == length ? EMBER_SUCCESS : EMBER_SERIAL_RX_EMPTY;
}
#endif // EMBER_TEST

Ecode_t COM_ReadPartialLine(COM_Port_t port, char *data, uint8_t max, uint8_t * index)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  Ecode_t err;
  uint8_t ch;

  if (((*index) == 0) || ((*index) >= max))
    data[0] = '\0';

  for (;;) {   
    err = COM_ReadByte(port, &ch);

    // no new serial port char?, keep looping
    if (err) return err;

    // handle bogus characters
    if ( ch > 0x7F ) continue;

    // handle leading newline - fogBUGZ # 584
    if (((*index) == 0) &&
        ((ch == '\n') || (ch == 0))) continue;

    // Drop the CR, or NULL that is part of EOL sequence.
    if ((*index) >= max) {
      *index = 0;
      if ((ch == '\r') || (ch == 0)) continue;
    }

    // handle backspace
    if ( ch == 0x8 || ch == 0x7F ) {
      if ( (*index) > 0 ) {
        // delete the last character from our string
        (*index)--;
        data[*index] = '\0';
        // echo backspace
        COM_WriteString(port, "\b \b");
      }
      // don't add or process this character
      continue;
    }

    //if the string is about to overflow, fake in a CR
    if ( (*index) + 2 > max ) {
      ch = '\r';
    }

    COM_WriteByte(port, ch); // term char echo

    //upcase that char
    if ( ch>='a' && ch<='z') ch = ch - ('a'-'A');

    // build a string until we press enter
    if ( ( ch == '\r' ) || ( ch == '\n' ) ) {
      data[*index] = '\0';

      if (ch == '\r') {
        COM_WriteByte(port, '\n'); // "append" LF
        *index = 0;                       // Reset for next line; \n next
      } else {
        COM_WriteByte(port, '\r'); // "append" CR
        *index = max;                     // Reset for next line; \r,\0 next
      }

      return EMBER_SUCCESS;
    } 
      
    data[(*index)++] = ch;
  }
}

Ecode_t COM_ReadLine(COM_Port_t port, char *data, uint8_t max)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  uint8_t index=0;

  while(COM_ReadPartialLine(port, data, max, &index) != EMBER_SUCCESS) {
    halResetWatchdog();
  }
  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Serial Output

// returns # bytes (if fifo mode)/messages (if buffer mode) that can be written
uint16_t COM_WriteAvailable(COM_Port_t port)  
{
  return comhandle[port]->txsize - comhandle[port]->txQueue->used;
}

uint16_t COM_WriteUsed(COM_Port_t port)
{
  return comhandle[port]->txQueue->used;
}

Ecode_t COM_WriteByte(COM_Port_t port, uint8_t dataByte)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  return COM_WriteData(port, &dataByte, 1);
}

Ecode_t COM_WriteHex(COM_Port_t port, uint8_t dataByte)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  uint8_t hex[2];
  emWriteHexInternal(hex, dataByte, 2);
  return COM_WriteData(port, hex, 2);
}

Ecode_t COM_WriteString(COM_Port_t port, PGM_P string)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  uint8_t *fifohead= &comhandle[port]->txQueue->fifo[comhandle[port]->txQueue->head];
  uint8_t length = 0;
  uint8_t wraplength = 0;
  while(*string != '\0') {
    while (! getOutputFifoSpace(port, 0)) {};
    INT_Disable();
    FIFO_ENQUEUE(comhandle[port]->txQueue,*string,comhandle[port]->txsize);
    INT_Enable();
    string++;
    length++;
    // queue just wrapped
    if (comhandle[port]->txQueue->head == 0)
    {
      // store first transmit length
      wraplength = length;
      // transmit chunk      
      if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
      {
        while(UARTDRV_Transmit(comhandle[port]->uarthandle, fifohead, wraplength, unloadTxBuffer) != EMBER_SUCCESS);
      }
#ifdef COM_VCP_ENABLE
      if (port == COM_VCP)
      {
          emDebugSendVuartMessage(comhandle[port]->txQueue->fifo, comhandle[port]->txsize);
          dequeueFifoBuffer(port, comhandle[port]->txsize);
      }
#endif //COM_VCP_ENABLE
      // move fifohead back to start
      fifohead = comhandle[port]->txQueue->fifo;
    }
  }  

  if ( length > wraplength)
  {
    if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
    {
      while(UARTDRV_Transmit(comhandle[port]->uarthandle, fifohead, length - wraplength, unloadTxBuffer) != EMBER_SUCCESS);
    }
  #ifdef COM_VCP_ENABLE
    if (port == COM_VCP)
    {
          emDebugSendVuartMessage(comhandle[port]->txQueue->fifo, comhandle[port]->txsize);
          dequeueFifoBuffer(port, comhandle[port]->txsize);
    }
  #endif //COM_VCP_ENABLE
  }
  return EMBER_SUCCESS;
}

Ecode_t COM_PrintCarriageReturn(COM_Port_t port)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  return COM_Printf(port, "\r\n");
}

Ecode_t COM_PrintfVarArg(COM_Port_t port, PGM_P formatString, va_list ap)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  Ecode_t stat = EMBER_SUCCESS; 
  if(!emPrintfInternal(COM_WriteData, port, formatString, ap))
    stat = EMBER_ERR_FATAL;
  return stat;
}

Ecode_t COM_Printf(COM_Port_t port, PGM_P formatString, ...)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  Ecode_t stat;
  va_list ap;
  va_start (ap, formatString);
  stat = COM_PrintfVarArg(port, formatString, ap);
  va_end (ap);
  return stat;
}

Ecode_t COM_PrintfLine(COM_Port_t port, PGM_P formatString, ...)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  Ecode_t stat;
  va_list ap;
  va_start (ap, formatString);
  stat = COM_PrintfVarArg(port, formatString, ap);
  va_end (ap);
  COM_PrintCarriageReturn(port);
  return stat;
}

Ecode_t COM_WriteData(COM_Port_t port, uint8_t *data, uint8_t length)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
#ifdef COM_VCP_ENABLE
  if ( port == COM_VCP )
  {
    emDebugSendVuartMessage(data, length);
    return EMBER_SUCCESS;
  }
#endif //COM_VCP_ENABLE

  uint8_t *fifohead= &comhandle[port]->txQueue->fifo[comhandle[port]->txQueue->head];
  uint8_t wraplength = 0;
  for (uint8_t i =0; i<length; i++) {
    while (! getOutputFifoSpace(port, 0)) {};
    INT_Disable();
    FIFO_ENQUEUE(comhandle[port]->txQueue,*data,comhandle[port]->txsize);
    INT_Enable();
    data++;
    // queue just wrapped
    if (comhandle[port]->txQueue->head == 0)
    {
      // store first transmit length
      wraplength = i + 1;
      // transmit chunk
      if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
      {
        while (UARTDRV_Transmit(comhandle[port]->uarthandle,fifohead, wraplength, unloadTxBuffer) != EMBER_SUCCESS);
      }
      // move fifohead back to start
      fifohead = comhandle[port]->txQueue->fifo;
    }
  }  
  if (length>wraplength)
  {
    if ( (port == COM_USART0) || (port == COM_USART1) || (port == COM_USART2) )
    {
      while(UARTDRV_Transmit(comhandle[port]->uarthandle, fifohead, length - wraplength, unloadTxBuffer) != EMBER_SUCCESS);
    }
  }
  return EMBER_SUCCESS;
}

Ecode_t COM_ForceWriteData(COM_Port_t port, uint8_t *data, uint8_t length)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
#ifdef COM_VCP_ENABLE
  if ( port == COM_VCP )
  {
    emDebugSendVuartMessage(data, length);
    return EMBER_SUCCESS;
  }
#endif //COM_VCP_ENABLE
  Ecode_t status;
  status = UARTDRV_ForceTransmit(comhandle[port]->uarthandle, data, length);
  return status;
}

Ecode_t COM_GuaranteedPrintf(COM_Port_t port, PGM_P formatString, ...)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  Ecode_t stat;
  va_list ap;
  va_start (ap, formatString);
  if(!emPrintfInternal(COM_ForceWriteData, port, formatString, ap))
    stat = EMBER_ERR_FATAL;
  va_end (ap);
  return stat;
}

Ecode_t COM_WaitSend(COM_Port_t port)
{
  if (checkValidPort(port)==false)
  {
    return EMBER_ERR_FATAL;
  }
  if (port == COM_VCP)
  {
    while (comhandle[port]->txQueue->used>0);
  }
  if ( (port==COM_USART0) || (port==COM_USART1) || (port==COM_USART2) )
  {
    while ( (comhandle[port]->txQueue->used > 0)
            || (UARTDRV_GetTransmitDepth(comhandle[port]->uarthandle) > 0)
            || !( (USART_StatusGet(comhandle[port]->uarthandle->initData.port) & _USART_STATUS_TXC_MASK) 
                  #ifdef _USART_STATUS_TXIDLE_MASK
                  && (USART_StatusGet(comhandle[port]->uarthandle->initData.port) & _USART_STATUS_TXIDLE_MASK)
                  #endif
                ) );
  }

  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Serial buffer maintenance
void COM_FlushRx(COM_Port_t port) 
{
  COM_FifoQueue_t *q = comhandle[port]->rxQueue;

  ATOMIC_LITE(
     q->used = 0;
     q->head = 0;
     q->tail = 0;
     )

  // halInternalRestartUart();

}

bool COM_Unused(uint8_t port)
{
  return !checkValidPort((COM_Port_t) port);
}
