/***************************************************************************//**
 * @file hal/micro/cortexm3/efm32/bootloader/btl-ezsp-spi-protocol.c
 * @brief EFM32 internal SPI protocol implementation for use with the 
 *   standalone ezsp spi bootloader.
 * @version
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Silicon Labs has no
 * obligation to support this Software. Silicon Labs is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Silicon Labs will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 ******************************************************************************/

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "btl-ezsp-spi-protocol.h"
#include "em_gpio.h"
#include "gpiointerrupt.h"
#include "em_usart.h"
#include "em_ldma.h"
#include "em_cmu.h"
#include "em_bus.h"
#include "em_int.h"
#include "em_chip.h"
#include "em_emu.h"
#include "spidrv.h"
#include "spi-protocol-device.h"

#if 0
  #define DEBUG_SET_LED()   halSetLed(BOARD_ACTIVITY_LED)
  #define DEBUG_CLEAR_LED() halClearLed(BOARD_ACTIVITY_LED)
#else
  #define DEBUG_SET_LED()
  #define DEBUG_CLEAR_LED()
#endif

#define SPIP_RESET               0x00
#define SPIP_OVERSIZED_EZSP      0x01
#define SPIP_ABORTED_TRANSACTION 0x02
#define SPIP_MISSING_FT          0x03
#define SPIP_UNSUPPORTED_COMMAND 0x04
#define SPIP_VERSION             0x82
#define SPIP_ALIVE               0xC1

#define FRAME_TERMINATOR 0xA7

#ifndef SPI_BTL_TIMEOUT
#define SPI_BTL_TIMEOUT 0
#endif

static const USART_InitSync_TypeDef initUsartData = SPI_BTL_USART_INIT;

SPIDRV_Init_t btlEzspSpi = SPI_NCP_USART_INIT;

// LDMA Assignments

// LDMA Rx and Tx Configurations
LDMA_TransferCfg_t periTransferRx = LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_USART1_RXDATAV);
LDMA_TransferCfg_t periTransferTx = LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_USART1_TXEMPTY);

// LDMA Rx and Tx Descriptors
LDMA_Descriptor_t rxDescriptor = LDMA_DESCRIPTOR_SINGLE_P2M_BYTE(&USART1->RXDATA,
                                                               (uint32_t)halHostCommandBuffer,
                                                               SPIP_BUFFER_SIZE);

LDMA_Descriptor_t txDescriptor = LDMA_DESCRIPTOR_SINGLE_M2P_BYTE((uint32_t)halHostResponseBuffer,
                                                               &USART1->TXDATA,
                                                               SPIP_BUFFER_SIZE);

const uint8_t rxChannel = 0;                // LDMA channel 0 for rx
const uint8_t txChannel = 1;                // LDMA channel 1 for tx

LDMA_Init_t initLdmaData = LDMA_INIT_DEFAULT;   // use default initialization


uint8_t halHostCommandBuffer[SPIP_BUFFER_SIZE];
uint8_t halHostResponseBuffer[SPIP_BUFFER_SIZE];
uint8_t spipErrorResponseBuffer[SPIP_ERROR_RESPONSE_SIZE];
//Provide easy references the buffers for EZSP
uint8_t *halHostCommandFrame = halHostCommandBuffer + EZSP_LENGTH_INDEX;
uint8_t *halHostResponseFrame = halHostResponseBuffer + EZSP_LENGTH_INDEX;

bool spipFlagWakeFallingEdge;    // detected a falling edge on nWAKE
bool spipFlagIdleHostInt;        // idle nHOST_INT at proper time
bool spipFlagOverrideResponse;   // just booted, or have an error to report
bool spipFlagTransactionActive;  // transaction is active
uint8_t spipResponseLength;      // true length of the Response Section


static void wipeAndRestartSpi(void);
static void configureSpiGpio(void);
static void halHostSerialPowerup(void);
static void setSpipErrorBuffer(uint8_t spiByte);
static void processSpipCommandAndRespond(uint8_t spipResponse);
static bool halInternalHostSerialTick(bool responseReady);
static void halGpioPolling(void);


static void wipeAndRestartSpi(void)
{ 
  spipFlagTransactionActive = false;
  spipResponseLength = 0;  

  // USART SPI initialization
  USART_InitSync(SPI_NCP_USART, &initUsartData);   // initialize the uart in spi mode
  
  SPI_NCP_USART->ROUTELOC0 = 0x00000000;
  SPI_NCP_USART->ROUTELOC0 |= (btlEzspSpi.portLocationRx << _USART_ROUTELOC0_RXLOC_SHIFT);
  SPI_NCP_USART->ROUTELOC0 |= (btlEzspSpi.portLocationTx << _USART_ROUTELOC0_TXLOC_SHIFT);
  SPI_NCP_USART->ROUTELOC0 |= (btlEzspSpi.portLocationClk << _USART_ROUTELOC0_CLKLOC_SHIFT);
  SPI_NCP_USART->ROUTELOC0 |= (btlEzspSpi.portLocationCs << _USART_ROUTELOC0_CSLOC_SHIFT);

  SPI_NCP_USART->ROUTEPEN |= USART_ROUTEPEN_RXPEN;
  SPI_NCP_USART->ROUTEPEN |= USART_ROUTEPEN_TXPEN;
  SPI_NCP_USART->ROUTEPEN |= USART_ROUTEPEN_CSPEN;
  SPI_NCP_USART->ROUTEPEN |= USART_ROUTEPEN_CLKPEN;

  LDMA->CH[0].DST = (uint32_t)halHostCommandBuffer;
  LDMA->CH[1].DST = (uint32_t)halHostResponseBuffer;
 
  SPI_NCP_USART->IEN |= USART_IEN_RXDATAV;    // enable rxdatav interrupt

  LDMA_IntClear(0xFFFFFFFF);           // clear channel done interrupts

  LDMA->CHEN    = 0;    // disable all channels
  LDMA->REQDIS  = 0;    // enable all peripheral requests

  NVIC_ClearPendingIRQ( LDMA_IRQn );

  // disable rx and tx in usart just in case so LDMA transfer won't start prematurely
  USART_Enable(SPI_NCP_USART, usartDisable);

  LDMA_StartTransfer(rxChannel,  &periTransferRx, &rxDescriptor);

  BUS_RegMaskedClear(&LDMA->CHEN, 1 << rxChannel);

  USART_Enable(SPI_NCP_USART, usartEnable); // enable the rx and tx in uart

  if(nSSEL_IS_NEGATED()){
    LDMA_IntEnable((uint32_t)1 << rxChannel);    // enable transfer complete interrupt for rxChannel
    BUS_RegMaskedSet(&LDMA->CHEN, 1 << rxChannel); //Enable receive channel.
  }
}

static void configureSpiGpio(void)
{
  GPIO_Port_TypeDef mosiPort, misoPort, clkPort;
  uint8_t mosiPin, misoPin, clkPin;

  if(SPI_NCP_USART == USART1){
    mosiPort = (GPIO_Port_TypeDef)AF_USART1_TX_PORT(btlEzspSpi.portLocationTx);
    misoPort = (GPIO_Port_TypeDef)AF_USART1_RX_PORT(btlEzspSpi.portLocationRx);
    clkPort  = (GPIO_Port_TypeDef)AF_USART1_CLK_PORT(btlEzspSpi.portLocationClk);
    mosiPin  = AF_USART1_TX_PIN(btlEzspSpi.portLocationTx);
    misoPin  = AF_USART1_RX_PIN(btlEzspSpi.portLocationRx);
    clkPin   = AF_USART1_CLK_PIN(btlEzspSpi.portLocationClk);
  }
  else if(SPI_NCP_USART == USART0){
    mosiPort = (GPIO_Port_TypeDef)AF_USART0_TX_PORT(btlEzspSpi.portLocationTx);
    misoPort = (GPIO_Port_TypeDef)AF_USART0_RX_PORT(btlEzspSpi.portLocationRx);
    clkPort  = (GPIO_Port_TypeDef)AF_USART0_CLK_PORT(btlEzspSpi.portLocationClk);
    mosiPin  = AF_USART0_TX_PIN(btlEzspSpi.portLocationTx);
    misoPin  = AF_USART0_RX_PIN(btlEzspSpi.portLocationRx);
    clkPin   = AF_USART0_CLK_PIN(btlEzspSpi.portLocationClk);
  }

  GPIO_PinModeSet(mosiPort, mosiPin, gpioModeInputPull, 1); // Configure MOSI
  GPIO_PinModeSet(misoPort, misoPin, gpioModePushPull, 0);  // Configure MISO
  GPIO_PinModeSet(clkPort, clkPin, gpioModeInputPull, 1);   // Configure SCK
  GPIO_PinModeSet(NSSEL_PORT, NSSEL_PIN, gpioModeInputPullFilter, 1); // configure SSEL
}

static void halHostSerialPowerup(void)
{
  CMU_HFXOInit_TypeDef hfxoInit = CMU_HFXOINIT_WSTK_DEFAULT;
  CMU_HFXOInit(&hfxoInit);
  CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(SPI_NCP_USART_CLOCK, true);

  LDMA_Init(&initLdmaData);

  // LDMA_Init always enables the IRQ, explicitly disable as bootloaders don't
  // handle interrupts
  NVIC_DisableIRQ( LDMA_IRQn );

  ////---- Configure SPI ----////
  wipeAndRestartSpi();

  ////---- Configure basic SPI Pins: MOSI, MISO, SCLK and nSSEL ----////
  configureSpiGpio();

  ////---- Configure nWAKE interrupt ----////
  //start from a fresh state just in case
#ifndef DISABLE_NWAKE
  GPIO->EXTIFALL = 0 << NWAKE_PIN; // disable triggering
  // configure nwake pin
  GPIO_IntConfig(NWAKE_PORT, NWAKE_PIN, false, true, false); // falling edge
  GPIO_PinModeSet(NWAKE_PORT, NWAKE_PIN, gpioModeInputPullFilter, 1); // input with pullup
#endif

  ////---- Configure nSSEL interrupt ----////
  GPIO->EXTIRISE = 0 << NSSEL_PIN; // disable triggering
  GPIO_IntConfig(NSSEL_PORT, NSSEL_PIN, true, false, false);

  ////---- Configure nHOST_INT output ----////
  GPIO_PinModeSet(NHOST_INT_PORT, NHOST_INT_PIN, gpioModePushPull, 1);

  // ----- Account for Noise and Crosstalk ------ //
  // on some hardware configurations there is a lot of noise and bootloading can fail
  // due to crosstalk. to avoid this, the slewrate is lowered here from 6 to 4, and the
  // drivestrength is lowered from 10mA to 1mA. if noise related errors still occur, 
  // the slewrate can be lowered further
  GPIO->P[NHOST_INT_PORT].CTRL = 0x00000000;
  GPIO->P[NHOST_INT_PORT].CTRL = (0x4 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
  GPIO->P[NHOST_INT_PORT].CTRL |= (0x4 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT);

  // the drivestrength is lowered from 10mA to 1mA by setting DRIVESTRENGTH to 1
  GPIO->P[NHOST_INT_PORT].CTRL |= (1 << _GPIO_P_CTRL_DRIVESTRENGTH_SHIFT);   
}

void halHostSerialInit(void)
{
  ////---- Initialize Flags ----////
  spipFlagWakeFallingEdge = false; //start with no edge on nWAKE
  spipFlagIdleHostInt = true;      //idle nHOST_INT after telling host we booted
  //load error response buffer with the "EM260 Reset" message + reset cause
  //we do not use the setSpipErrorBuffer() function here since that function
  //assumes the second byte in the buffer is reserved (0)
  spipFlagOverrideResponse = true; //set a flag indicating we just booted
  spipErrorResponseBuffer[0] = SPIP_RESET;
  spipErrorResponseBuffer[1] = EM2XX_RESET_BOOTLOADER; //reset is always bootloader

  halHostSerialPowerup();
}

void halHostCallback(bool haveData)
{
  if(haveData) {
    //only assert nHOST_INT if we are outside a wake handshake (wake==1)
    //and outside of a current transaction (nSSEL=1)
    //if inside a wake handshake or transaction, delay asserting nHOST_INT
    //until the SerialTick
    if( nWAKE_IS_NEGATED() && nSSEL_IS_NEGATED() ) {
      GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN);
    }
    spipFlagIdleHostInt = false;
  } else {
    spipFlagIdleHostInt = true;
  }
}

static void setSpipErrorBuffer(uint8_t spiByte)
{
  if(!spipFlagOverrideResponse) {
    //load error response buffer with the error supplied in spiByte
    spipFlagOverrideResponse = true;      //set a flag indicating override
    spipErrorResponseBuffer[0] = spiByte; //set the SPI Byte with the error
    spipErrorResponseBuffer[1] = 0;       //this byte is currently reserved
  }
}

static void processSpipCommandAndRespond(uint8_t spipResponse)
{
  // Disable reception while processing
  BUS_RegMaskedClear(&LDMA->CHEN, 1 << rxChannel);
  //check for Frame Terminator, it must be there!
  if(halHostCommandBuffer[1]==FRAME_TERMINATOR) {
    //override with the supplied spipResponse
    halHostResponseBuffer[0] = spipResponse;
  } else {
    //no frame terminator found!  report missing F.T.
    setSpipErrorBuffer(SPIP_MISSING_FT);
  }
  halHostSerialTick(true); //respond immediately!
}

uint8_t getSpipResponseLength(void){
  return spipResponseLength;
}

// One layer of indirection is used so calling the public function will actually
// result in the real Tick function (this internal one) being wrapped in an
// ATOMIC() block to prevent potential corruption from the nSSEL interrupt.
static bool halInternalHostSerialTick(bool responseReady)
{
  //assert nHOST_INT if need to tell host something immediately and nSSEL=idle
  if(spipFlagOverrideResponse && nSSEL_IS_NEGATED()) {
    GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN);  //tell the host we just got into bootloader
  }

  if(spipFlagWakeFallingEdge){ // detected falling edge on nWAKE, handshake
    GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN);
    while(nWAKE_IS_ASSERTED()) {halResetWatchdog(); /*EMHAL-1074*/}
    GPIO_PinOutSet(NHOST_INT_PORT, NHOST_INT_PIN);
    spipFlagWakeFallingEdge = false;
    // The wake handshake is complete, but spipFlagIdleHostInt is saying
    // that there is a callback pending.
    if(!spipFlagIdleHostInt){
      halCommonDelayMicroseconds(50); // delay 50us so host can get ready
      GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN); // indicate the pending callback
    }
  }
  else if(responseReady && spipFlagTransactionActive) { // OK to transmit
    if(spipFlagOverrideResponse){
      spipFlagOverrideResponse = false; // we no longer need to override
      // override whatever was sent with the error response message
      MEMCOPY(halHostResponseBuffer,
              spipErrorResponseBuffer,
              SPIP_ERROR_RESPONSE_SIZE);
    }
    if(spipFlagIdleHostInt) {
      GPIO_PinOutSet(NHOST_INT_PORT, NHOST_INT_PIN); // the nHOST_INT signal can be asynchronously
    }
    // add Frame terminator and record true response length
    if(halHostResponseBuffer[0]<0x05){
      halHostResponseBuffer[1+1] = FRAME_TERMINATOR;
      spipResponseLength = 3; // true response length
    }
    else if((halHostResponseBuffer[0]==0xFE) || //EZSP payload
            (halHostResponseBuffer[0]==0xFD)) { // bootloader payload
      halHostResponseBuffer[halHostResponseBuffer[1] +1 +1] = FRAME_TERMINATOR;
      spipResponseLength = halHostResponseBuffer[1]+3; // true response length
    }
    else {
      halHostResponseBuffer[1] = FRAME_TERMINATOR;
      spipResponseLength = 2; // true response length
    }
    
    LDMA_StartTransfer(txChannel,  &periTransferTx, &txDescriptor);
    
    GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN); // tell host to get the response
  }
  else if(spipResponseLength == 0){ // no data to transmit, pump receive side
    // activate receive if not already and nSSEL is inactive
    if( (BUS_RegMaskedRead(&LDMA->CHEN, 1 << rxChannel) == 0) 
        && nSSEL_IS_NEGATED()) {
      volatile uint8_t dummy;
      // flush RX FIFO since the wait and response section overflowed it
      dummy = (uint8_t)SPI_NCP_USART->RXDATA;
      dummy = (uint8_t)SPI_NCP_USART->RXDATA;
      dummy = (uint8_t)SPI_NCP_USART->RXDATA;

      LDMA_IntEnable(1 << rxChannel);    // enable transfer complete int for rxChannel
      BUS_RegMaskedSet(&LDMA->CHEN, 1 << rxChannel);  //Enable receive channel.
    }
    // check for valid start of data (CH[0].DST has moved from start)
    // check for unloaded buffer
    if (BUS_RegMaskedRead(&LDMA->CH[0].DST, 0xFFFFFFFF) > (uint32_t)&halHostCommandBuffer 
        || BUS_RegMaskedRead(&LDMA->CHDONE, 1 << rxChannel)){      
      spipFlagTransactionActive = true; // RX'ed, definitely in a transaction
      GPIO_PinOutSet(NHOST_INT_PORT, NHOST_INT_PIN); // by clocking a byte, the Host ack'ed nHOST_INT
      
      // we need at least 2 bytes before processing the command
      // check that DST has moved at least 2 places
      if(BUS_RegMaskedRead(&LDMA->CH[0].DST, 0xFFFFFFFF) >= 
          ((uint32_t)&halHostCommandBuffer + 0x2)){ 
        // take action depending on the command
        switch(halHostCommandBuffer[0]) {
          case 0x0A:
            processSpipCommandAndRespond(SPIP_VERSION);
            break;
          case 0x0B:
            processSpipCommandAndRespond(SPIP_ALIVE);
            break;
          case 0xFE: // the command is an EZSP frame
            // fall into bootloader frame since processing the rest of the
            // command is the same. The only difference is responding with the 
            // unsupported SPI command error
          case 0xFD: // the command is a bootloader frame
            // guard against oversized message which could cause serious problems
            if(halHostCommandBuffer[1] > MAX_PAYLOAD_FRAME_LENGTH) {
              wipeAndRestartSpi();
              setSpipErrorBuffer(SPIP_OVERSIZED_EZSP);
              return false;
            }
            // check for all data before responding that we have a valid buffer
            if(BUS_RegMaskedRead(&LDMA->CH[0].DST, 0xFFFFFFFF) >= 
                ((uint32_t)&halHostCommandBuffer + halHostCommandBuffer[1] + 0x3)){
              
              //disable reception while processing
              BUS_RegMaskedClear(&LDMA->CHEN, 1 << rxChannel);  

              // check for frame terminator, it must be there!
              if(spipFlagOverrideResponse) {
                halHostSerialTick(true); // respond immediately!
                return false; // we overrode the command
              }
              else if(halHostCommandBuffer[halHostCommandBuffer[1]+2] != FRAME_TERMINATOR) {
                // no frame terminator found! report missing frame terminator
                setSpipErrorBuffer(SPIP_MISSING_FT);
                halHostSerialTick(true); // resond immediately!
                return false; // we overrode the command
              }
              else if(halHostCommandBuffer[0]==0xFE){
                // load error response buffer with unsupported SPI command error
                setSpipErrorBuffer(SPIP_UNSUPPORTED_COMMAND);
                halHostSerialTick(true); // respond immediately!
                return false; // we overrode the command
              }
              else {
                halHostResponseBuffer[0] = 0xFD; // mark respond bootloader frame
                return true; // there is a valid command
              }
            }
            break;
          default:
            break;
        }
      }
    }
  }
  return false;
}

//Since the bootloader does not use interrupts, the upper layer must use this
//function to poll for the status of the SPI Protocol pins.
static void halGpioPolling(void)
{
  //polling for the first byte on MOSI
  if((BUS_RegMaskedRead(&LDMA->CH[0].DST, 0xFFFFFFFF) > (uint32_t)&halHostCommandBuffer) && (spipResponseLength <= 0)){
    GPIO_PinOutSet(NHOST_INT_PORT, NHOST_INT_PIN); //by clocking a byte, the Host ack'ed nHOST_INT
    spipFlagTransactionActive = true; //RX'ed, definitely in a transaction
  }
  
#ifndef DISABLE_NWAKE
  //polling for the falling edge of the nWAKE
  //this fakes out halGpioIsr() in the normal SPI Protocol
  if((GPIO_IntGet()&((uint32_t)1<<NWAKE_PIN))==((uint32_t)1<<NWAKE_PIN)) {
    // ack int before read to avoid potential of missing interrupt
    GPIO_IntClear((uint32_t)1 << NWAKE_PIN);  // clears the nWAKE interrupt flag
    
    //A wakeup handshake should be performed in response to a falling edge on
    //the WAKE line. The handshake should only be performed on a SerialTick.
    spipFlagWakeFallingEdge = true;
  }
#endif
  
  //polling for the rising edge of nSSEL
  //this fakes out halTimerIsr() in the normal SPI Protocol
  if((GPIO_IntGet()&((uint32_t)1<<NSSEL_PIN))==((uint32_t)1<<NSSEL_PIN)) {
    // ack int before read to avoid potential of missing interrupt
    GPIO_IntClear((uint32_t)1 << NSSEL_PIN);  // clears the nSSEL interrupt flag
    //normally nHOST_INT is idled in the RXVALID Isr, but with short and fast
    //Responses, it's possible to service nSSEL before RXVALID, but we
    //still need to idle nHOST_INT.  If there is a pending callback, it will be
    //alerted via nHOST_INT at the end of this function.
    GPIO_PinOutSet(NHOST_INT_PORT, NHOST_INT_PIN);

    //It's always safer to wipe the SPI clean and restart between transactions
    wipeAndRestartSpi();
    
    if(!spipFlagIdleHostInt) {
      GPIO_PinOutClear(NHOST_INT_PORT, NHOST_INT_PIN); //we still have more to tell the Host
    }
  }
}

bool halHostSerialTick(bool responseReady)
{
  // bool validCommand;
  //Processing a potential premature nSSEL deactivation inside of the Tick
  //function will generate a lot of strange conditions that are best prevented
  //insteaded of handled.
  //Normal calls to halInternalHostSerialTick are <10us.  Worst case is <30us.
  //NOTE - The normal SPI Protocol wraps this call with an ATOMIC due to the
  //       above reason.  Since the bootloader does not use interrupts, we do
  //       not need to wrap this call in an ATOMIC.
  // ATOMIC(
  //   validCommand = halInternalHostSerialTick(responseReady);
  // )
  // return validCommand;
  halGpioPolling();
  return halInternalHostSerialTick(responseReady);
}
