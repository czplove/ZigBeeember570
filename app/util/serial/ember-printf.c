/**
 * File: ember-printf.c
 * Description: Minimalistic implementation of printf().
 *
 *
 * Copyright 2014 by Silicon Labs.  All rights reserved.                   *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"

//Host processors do not use Ember Message Buffers.
#ifndef EZSP_HOST
  #include "stack/include/packet-buffer.h"
#endif

#include "hal/hal.h"
#include "serial.h"

#if CORTEXM3_EFM32_MICRO
#include "em_device.h"
#include "com.h"
#endif
#include "ember-printf.h"

#include <stdarg.h>

#ifdef EMBER_SERIAL_USE_STDIO
#include <stdio.h>
#endif //EMBER_SERIAL_USE_STDIO

#ifdef EMBER_SERIAL_CUSTOM_STDIO
#include EMBER_SERIAL_CUSTOM_STDIO
#define EMBER_SERIAL_USE_STDIO
#endif // EMBER_SERIAL_CUSTOM_STDIO

//=============================================================================
// Globals

// --------------------------------
// A simple printf() implementation
// Supported format specifiers are:
//  %% - percent sign
//  %c - single byte character
//  %s - ram string
//  %p - flash string  (non-standard)
//  %u - 2-byte unsigned decimal
//  %d - 2-byte signed decimal
//  %x %2x %4x - 1, 2, 4 BYTE hex value (always 0 padded) (non-standard)
//    Non-standard behavior: Normally a number after a % is interpreted to be
//    a minimum character width, and the value is not zero padded unless
//    there is a zero before the minimum width value.
//    i.e. '%2x' for the uint16_t value 0xb prints " b", while '%02x' would print
//    "0b".
//    Ember assumes the number after the % and before the 'x' to be the number
//    of BYTES, and all hex values are left-justified zero padded.
// 
// A few macros and a function help make this readable:
//   - flush the local buffer to the output
//   - ensure that there is some room in the local buffer
//   - add a single byte to the local buffer
//   - convert a nibble to its ascii hex character
//   - convert an uint16_t to a decimal string
// Most of these only work within the emPrintfInternal() function.

// Current champion is %4x which writes 8 bytes.  (%s and %p can write
// more, but they do their own overflow checks).
#if CORTEXM3_EFM32_MICRO
#define LOCAL_BUFFER_SIZE 64
#else
#define LOCAL_BUFFER_SIZE 16
#endif
#define MAX_SINGLE_COMMAND_BYTES 8

static PGM uint32_t powers10[9] = {
  1000000000,
  100000000,
  10000000,
  1000000,
  100000,
  10000,
  1000,
  100,
  10
};

#define flushBuffer() \
do { count = localBufferPointer - localBuffer;     \
     if (flushHandler(port, localBuffer, count) != EMBER_SUCCESS) \
       goto fail;                                  \
     total += count;                               \
     localBufferPointer = localBuffer;             \
     (void)localBufferPointer;                     \
} while (false)                                           

#define addByte(byte) \
do { *(localBufferPointer++) = (byte); } while (false)

//=============================================================================

uint8_t *emWriteHexInternal(uint8_t *charBuffer, uint16_t value, uint8_t charCount)
{
  uint8_t c = charCount;
  charBuffer += charCount;
  for (; c; c--) {
    uint8_t n = value & 0x0F;
    value = value >> 4;
    *(--charBuffer) = n + (n < 10
                           ? '0'
                           : 'A' - 10);
  }
  return charBuffer + charCount;
}


// This function will write a decimal ASCII string to buffer 
// containing the passed 'value'.  Includes negative sign, if applicable.
// Returns the number of bytes written.

static uint8_t decimalStringWrite(int32_t value, uint8_t* buffer)
{
  uint8_t length = 0;

  // We introduce this variable to accomodate the actual value to be printed.
  // This is necessary for handling the case in which we print -2147483648,
  // since we need to flip the sign and an int32_t can represent up to 2147483647
  // while an int32_t can represent an integer as big as 4294967295.
  uint32_t printValue = (uint32_t)value;

  // If the most significant bit is set to 1, i.e., if value is negative.
  if (value & 0x80000000L)
  {
    buffer[length++] = '-';

    // Since we are assigning to an uint32_t we can safetly flip the sign and get
    // the absolute value.
    printValue = -value;
  }

  {
    uint8_t i;
    bool printedLeadingNonZeroValue = false;
    // To prevent using 32-bit divide or modulus,
    // since those operations are expensive on a 16-bit processor,
    // we use subtraction and a constant array with powers of 10.
    for (i = 0; i < 9; i++) {
      uint8_t digit = 0;
      while (printValue >= powers10[i]) {
        printValue -= powers10[i];
        digit++;
      }
      if (digit != 0 || printedLeadingNonZeroValue) {
        buffer[length++] = '0' + digit;
        printedLeadingNonZeroValue = true;
      }
    }
    buffer[length++] = '0' + printValue;

    return length;
  }
}


// Returns number of characters written
uint8_t emPrintfInternal(emPrintfFlushHandler flushHandler, 
                       COM_Port_t port,
                       PGM_P string, 
                       va_list args)
{
  uint8_t localBuffer[LOCAL_BUFFER_SIZE + MAX_SINGLE_COMMAND_BYTES];
  uint8_t *localBufferPointer = localBuffer;
  uint8_t *localBufferLimit = localBuffer + LOCAL_BUFFER_SIZE;
  uint8_t count;
  uint8_t total = 0;
  bool stillInsideFormatSpecifier = false;

  for (; *string; string++) {
    uint8_t next = *string;
    if (next != '%' && !(stillInsideFormatSpecifier))
      addByte(next);
    else {
      if (stillInsideFormatSpecifier) {
        stillInsideFormatSpecifier = false;
      } else {
        string += 1;
      }
      switch (*string) {
      case '%':
        // escape for printing "%"
        addByte('%');
        break;
      case 'c':
        // character
        addByte(va_arg(args, unsigned int) & 0xFF);
        break;
      case 'p': 
        // only avr needs to special-case the pgm handling, all other current
        //  platforms fall through to standard string handling.
        #ifdef AVR_ATMEGA
          {
          // flash string
          PGM_P arg = va_arg(args, PGM_P);
          while (true) {
            uint8_t ch = *arg++;
            if (ch == '\0')
              break;
            *(localBufferPointer++) = ch;
            if (localBufferLimit <= localBufferPointer)
              flushBuffer();
          }
          break;
        }
        #endif
      case 's': {
        // string
        uint8_t len;
        uint8_t *arg = va_arg(args, uint8_t *);
        flushBuffer();
        for (len=0; arg[len] != '\0'; len++) {};
        if (flushHandler(port, arg, len) != EMBER_SUCCESS)
          goto fail;
        total += len;
        break; }

      // Note: We don't support printing unsigned 32-bit values.
      case 'l':         // signed 4-byte
      case 'u':         // unsigned 2-byte
      case 'd': {       // signed 2-byte
        int32_t value;
        if (*string == 'l') {
          value = va_arg(args, long int);
        } else if (*string == 'u') { // Need when sizeof(int) != sizeof(uint16_t)
          value = va_arg(args, unsigned int);
        } else {
          value = va_arg(args, int);
        }
        localBufferPointer += decimalStringWrite(value, localBufferPointer);
        break;
      }
      case 'x':
      case 'X': {
        // single hex byte (always prints 2 chars, ex: 0A)
        uint8_t data = va_arg(args, int);
       
        localBufferPointer = emWriteHexInternal(localBufferPointer, data, 2);
        break; }
      
      case '0':
      case '1':
      case '3':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        // We don't support width specifiers, but we want to accept
        // them so they can be ignored and thus we are compatible with
        // code that tries to use them.

      case '2':
        // %2x only, 2 hex bytes (always prints 4 chars)
      case '4':
        // %4x only, 4 hex bytes (always prints 8 chars)

        stillInsideFormatSpecifier = true;
        if (*(string + 1) == 'x' || *(string + 1) == 'X') {
          string++;
          stillInsideFormatSpecifier = false;
          if (*(string - 1) == '2') {
            uint16_t data = va_arg(args, int);
            localBufferPointer = emWriteHexInternal(localBufferPointer, data, 4);
          } else if (*(string - 1) == '4') {
            uint32_t data = va_arg(args, uint32_t);
            // On the AVR at least, the code size is smaller if we limit the
            // emWriteHexInternal() code to 16-bit numbers and call it twice in
            // this case.  Other processors may have a different tradeoff.
            localBufferPointer = emWriteHexInternal(localBufferPointer, 
                                                    (uint16_t) (data >> 16), 
                                                    4);
            localBufferPointer = emWriteHexInternal(localBufferPointer, 
                                                    (uint16_t) data, 
                                                    4);
          } else {
            stillInsideFormatSpecifier = true;
            string--;
          }
        } 

        break;
      case '\0':
        goto done;
      default: {
      }
      } //close switch.
    }
    if (localBufferLimit <= localBufferPointer)
      flushBuffer();
  }
  
 done:
  flushBuffer();
  return total;

 fail:
  return 0;
}

#if CORTEXM3_EFM32_MICRO

EmberStatus emberSerialWriteHex(uint8_t port, uint8_t dataByte)
{
  return COM_WriteHex((COM_Port_t) port, dataByte);
}

EmberStatus emberSerialPrintCarriageReturn(uint8_t port)
{
  return emberSerialPrintf(port, "\r\n");
}

EmberStatus emberSerialPrintf(uint8_t port, PGM_P formatString, ...)
{
  Ecode_t stat;
  va_list ap;
  va_start (ap, formatString);
  stat = COM_PrintfVarArg((COM_Port_t) port, formatString, ap);
  va_end (ap);
  return stat;
}

EmberStatus emberSerialPrintfLine(uint8_t port, PGM_P formatString, ...)
{
  EmberStatus stat;
  va_list ap;
  va_start (ap, formatString);
  stat = COM_PrintfVarArg((COM_Port_t) port, formatString, ap);
  va_end (ap);
  COM_Printf((COM_Port_t) port, "\r\n");
  return stat;
}
EmberStatus emberSerialPrintfVarArg(uint8_t port, PGM_P formatString, va_list ap)
{
  return COM_PrintfVarArg((COM_Port_t) port, formatString, ap);
}

#else //CORTEXM3_EFM32_MICRO

EmberStatus emberSerialWriteHex(uint8_t port, uint8_t dataByte)
{
  uint8_t hex[2];
  emWriteHexInternal(hex, dataByte, 2);
  return emberSerialWriteData(port, hex, 2);
}

EmberStatus emberSerialPrintCarriageReturn(uint8_t port)
{
  return emberSerialPrintf(port, "\r\n");
}

EmberStatus emberSerialPrintfVarArg(uint8_t port, PGM_P formatString, va_list ap)
{
   EmberStatus stat = EMBER_SUCCESS;
   
#ifdef EMBER_SERIAL_USE_STDIO
  if(!emPrintfInternal(emberSerialWriteData, port, formatString, ap)) {
    stat = EMBER_ERR_FATAL;
  }
#else //EMBER_SERIAL_USE_STDIO
  
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO: {
    if(!emPrintfInternal(emberSerialWriteData, port, formatString, ap))
      stat = EMBER_ERR_FATAL;
    break;
  }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER: {
    EmberMessageBuffer buff = emberAllocateStackBuffer();
    if(buff == EMBER_NULL_MESSAGE_BUFFER) {
      stat = EMBER_NO_BUFFERS;
      break;
    }
    if(emPrintfInternal(emberAppendToLinkedBuffers,
                        buff,
                        formatString,
                        ap)) {
      stat = emberSerialWriteBuffer(port,buff,0,emberMessageBufferLength(buff));
    } else {
      stat = EMBER_NO_BUFFERS;
    }
    // Refcounts may be manipulated in ISR if DMA used
    ATOMIC( emberReleaseMessageBuffer(buff); )

    break;
  }
#endif
  default: {
  }
  } //close switch.
#endif //EMBER_SERIAL_USE_STDIO
  return stat;
}

EmberStatus emberSerialPrintf(uint8_t port, PGM_P formatString, ...)
{
  EmberStatus stat;
  va_list ap;
  va_start (ap, formatString);
  stat = emberSerialPrintfVarArg(port, formatString, ap);
  va_end (ap);
  return stat;
}

EmberStatus emberSerialPrintfLine(uint8_t port, PGM_P formatString, ...)
{
  EmberStatus stat;
  va_list ap;
  va_start (ap, formatString);
  stat = emberSerialPrintfVarArg(port, formatString, ap);
  va_end (ap);
  emberSerialPrintCarriageReturn(port);
  return stat;
}
#endif //CORTEXM3_EFM32_MICRO
