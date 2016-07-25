/*
 * File: btl-ezsp-spi-protocol.h
 * Description: em35x standalone bootloader public definitions
 *
 *
 * Copyright 2009 by Ember Corporation. All rights reserved.                *80*
 */


#ifndef __BTL_EZSP_SPI_PROTOCOL_H__
#define __BTL_EZSP_SPI_PROTOCOL_H__
#ifdef BTL_HAS_EZSP_SPI

void halHostSerialInit(void);
bool halHostSerialTick(bool responseReady);
void halHostCallback(bool haveData);
void spiImmediateResponse(uint8_t status);

extern uint8_t halHostResponseBuffer[];
extern uint8_t halHostCommandBuffer[];

void flushSpiResponse(void);

extern bool spipFramePending;      // halHostBuffer[] contains a frame
extern bool spipResponsePending;   // have data to send in next spi session

#define XMODEM_FRAME_START_INDEX 2

#define SPI_FRAME_ID_INDEX 0
#define SPI_FRAME_LENGTH_INDEX 1

// Note these lengths only apply directly to radio messages
// For ezsp spi the length is decreased by RSP_MSG_ACK_NAK_ERR_OFFSET + 1
#define QUERY_RESPONSE_MESSAGE_LENGTH 53

// The EM260 added an extra byte with error information to ACK, NAK and CAN
// messages. To add a zero byte so that the message length remain the same,
// define THREE_BYTE_ACK_NAK_CAN_MESSAGES.
//#define THREE_BYTE_ACK_NAK_CAN_MESSAGES
#ifdef THREE_BYTE_ACK_NAK_CAN_MESSAGES
  #define QUERY_ACK_NAK_MESSAGE_LENGTH  30
#else
  #define QUERY_ACK_NAK_MESSAGE_LENGTH  29
#endif

#endif // BTL_HAS_EZSP_SPI
#endif //__BTL_EZSP_SPI_PROTOCOL_H__
