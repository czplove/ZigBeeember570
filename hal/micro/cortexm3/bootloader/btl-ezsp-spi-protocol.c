/** @file hal/micro/micro/cortexm3/btl-ezsp-spi-protocol.c
 *  @brief  EM3XX internal SPI Protocol implementation for use with the
 *  standalone ezsp spi bootloader.
 * 
 * 
 * SPI Protocol Interface:
 * 
 * void halHostSerialInit(void)
 * void halHostSerialPowerup(void)
 * void halHostSerialPowerdown(void)
 * void halHostCallback(bool haveData)
 * bool halHostSerialTick(bool responseReady)
 * void halGpioPolling(void)
 * uint8_t *halHostCommandFrame
 * uint8_t *halHostResponseFrame
 * 
 * 
 * <!-- Author(s): Brooks Barrett -->
 * <!-- Copyright 2007, 2009 by Ember Corporation. All rights reserved.  *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "btl-ezsp-spi-protocol.h"
#include "hal/micro/cortexm3/ezsp-spi-serial-controller-autoselect.h"

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
//The maximum SPI Protocol message size is 136 bytes. We define a buffer of
//142 specifically for error detection during the Response Section.  By using
//a buffer of 142, we can use the SCx_TXCNT register to monitor the state of
//the transaction and know that if a DMA TX unload occurs we have an error.
#define SPIP_BUFFER_SIZE             142
#define SPIP_ERROR_RESPONSE_SIZE     2
#define MAX_PAYLOAD_FRAME_LENGTH     133
#define EZSP_LENGTH_INDEX            1
#define RX_DMA_BYTES_LEFT_THRESHOLD  4
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
uint8_t spipResponseLength;           // true length of the Response Section

static void halHostSerialPowerup(void);

static void wipeAndRestartSpi(void)
{
  spipFlagTransactionActive = false;//we're definitely outside a Transaction now
  spipResponseLength = 0;           //default length of zero
  
  // Make SPI peripheral clean and start a-new
  INT_SCxCFG &= ~INT_SCRXVAL;   //disable byte received interrupt
  INT_SCxCFG &= ~INT_SCRXULDA;  //disable buffer A unload interrupt
  SCx_REG(DMACTRL) = SC_RXDMARST;    //reset DMA just in case
  SCx_REG(DMACTRL) = SC_TXDMARST;    //reset DMA just in case
  SCx_REG(MODE) = SCx_MODE(DISABLED); //be safe, make sure we start from disabled
  SCx_REG(RATELIN) =  0; //no effect in slave mode
  SCx_REG(RATEEXP) =  0; //no effect in slave mode
  SCx_REG(SPICFG)  =  (0 << SC_SPIMST_BIT)  | //slave mode
                      (0 << SC_SPIPHA_BIT)  | //SPI Mode 0 - sample leading edge
                      (0 << SC_SPIPOL_BIT)  | //SPI Mode 0 - rising leading edge
                      (0 << SC_SPIORD_BIT)  | //MSB first
                      (0 << SC_SPIRXDRV_BIT)| //no effect in slave mode
                      (0 << SC_SPIRPT_BIT)  ; //transmit 0xFF when no data to send
  SCx_REG(MODE)   =  SCx_MODE(SPI); //activate SPI mode
  //Configure DMA RX channel to point to the command buffer
  SCx_REG(RXBEGA) = (uint32_t) halHostCommandBuffer;
  SCx_REG(RXENDA) = (uint32_t) halHostCommandBuffer + SPIP_BUFFER_SIZE -1;
  //Configure DMA TX channel to point to the response buffer
  SCx_REG(TXBEGA) = (uint32_t) halHostResponseBuffer;
  SCx_REG(TXENDA) = (uint32_t) halHostResponseBuffer + SPIP_BUFFER_SIZE -1;
  if(nSSEL_IS_HIGH) { //only activate DMA if nSSEL is idle
    //since bootloader is polling driven, do not enable ISRs!
    INT_SCxCFG |= INT_SCRXVAL; //enable byte received interrupt
    INT_SCxCFG |= INT_SCRXULDA;//enable RX buffer A unload interrupt
    SCx_REG(DMACTRL) = SC_RXLODA;  //activate RX DMA for first command
  }
  INT_SCxFLAG = 0xFFFF;     //clear any stale interrupts
  //since bootloader is polling driven, do not enable top-level SCx interrupts!
//  INT_CFGSET = INT_SCx;     // no interrupts in bootloader!
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

static void halHostSerialPowerup(void)
{
  //On the next Tick call, the SPIP can assume we are fully booted and ready.
  
  ////---- Configure SPI ----////
    wipeAndRestartSpi();

  ////---- Configure basic SPI Pins: MOSI, MISO, SCLK and nSSEL ----////
    CFG_SPI_GPIO();
    PULLUP_nSSEL();

  ////---- Configure nWAKE interrupt ----////
    //start from a fresh state just in case
    INT_CFGCLR = nWAKE_INT;               //disable triggering
    nWAKE_INTCFG = (GPIOINTMOD_DISABLED << GPIO_INTMOD_BIT);
    //Configure nWAKE pin
    CFG_nWAKE(GPIOCFG_IN_PUD);            //input with pullup
    PULLUP_nWAKE();
    //Enable Interrupts
    INT_GPIOFLAG = nWAKE_GPIOFLAG;        //clear stale interrupts
    INT_PENDCLR = nWAKE_INT;
//    INT_CFGSET = nWAKE_INT;             // no interrupts in bootloader!
    nWAKE_INTCFG =  (0 << GPIO_INTFILT_BIT) |   //no filter
                    (GPIOINTMOD_FALLING_EDGE << GPIO_INTMOD_BIT);
  
  ////---- Configure nSSEL_INT for compatibility with EM260 ----////
    CFG_nSSEL_INT(GPIOCFG_IN);            //input floating - not used

  ////---- Configure nSSEL interrupt (IRQC) ----////
    INT_CFGCLR = nSSEL_INT;               //disable triggering
    nSSEL_INTCFG = (GPIOINTMOD_DISABLED << GPIO_INTMOD_BIT);
    nSSEL_IRQSEL = nSSEL_IRQSEL_MASK;     //assign nSSEL pin to IRQC
    //Enable Interrupts
    INT_GPIOFLAG = nSSEL_GPIOFLAG;        //clear stale interrupts
    INT_PENDCLR = nSSEL_INT;
//    INT_CFGSET = nSSEL_INT;             // no interrupts in bootloader!
    nSSEL_INTCFG = (0 << GPIO_INTFILT_BIT) |  //no filter
                   (GPIOINTMOD_RISING_EDGE << GPIO_INTMOD_BIT);

  ////---- Configure nHOST_INT output ----////
    SET_nHOST_INT();
    CFG_nHOST_INT(GPIOCFG_OUT);
}

void halHostCallback(bool haveData)
{
  if(haveData) {
    //only assert nHOST_INT if we are outside a wake handshake (wake==1)
    //and outside of a current transaction (nSSEL=1)
    //if inside a wake handshake or transaction, delay asserting nHOST_INT
    //until the SerialTick
    if( nWAKE_IS_HIGH && nSSEL_IS_HIGH ) {
      CLR_nHOST_INT();
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
  DEBUG_SET_LED();//show me when stopped receiving
  SCx_REG(DMACTRL) = SC_RXDMARST; //disable reception while processing
  DEBUG_CLEAR_LED();
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

//One layer of indirection is used so calling the public function will actually
//result in the real Tick function (this internal one) being wrapped in an
//ATOMIC() block to prevent potential corruption from the nSSEL interrupt.
static bool halInternalHostSerialTick(bool responseReady)
{
  //assert nHOST_INT if need to tell host something immediately and nSSEL=idle
  if(spipFlagOverrideResponse && nSSEL_IS_HIGH) {
    CLR_nHOST_INT();  //tell the host we just got into bootloader
  }
  
  if(spipFlagWakeFallingEdge) { //detected falling edge on nWAKE, handshake
    CLR_nHOST_INT();
    while( nWAKE_IS_LOW ) { halResetWatchdog(); /*EMHAL-1074*/ }
    SET_nHOST_INT();
    spipFlagWakeFallingEdge = false;
    //The wake handshake is complete, but spipFlagIdleHostInt is saying
    //that there is a callback pending.
    if(!spipFlagIdleHostInt) {
      halCommonDelayMicroseconds(50); //delay 50us so Host can get ready
      CLR_nHOST_INT();  //indicate the pending callback
    }
  } else if(responseReady && spipFlagTransactionActive) {  //OK to transmit
    DEBUG_SET_LED();
    if(spipFlagOverrideResponse) {
      spipFlagOverrideResponse = false; //we no longer need to override
      //override whatever was sent with the error response message
      MEMCOPY(halHostResponseBuffer,
              spipErrorResponseBuffer,
              SPIP_ERROR_RESPONSE_SIZE);
    }
    if(spipFlagIdleHostInt) {
      SET_nHOST_INT();  //the nHOST_INT signal can be asynchronously
    }
    //add Frame Terminator and record true Response length
    if( halHostResponseBuffer[0]<0x05 ) {
      halHostResponseBuffer[1 +1] = FRAME_TERMINATOR;
      spipResponseLength = 3;  //true Response length
    } else if((halHostResponseBuffer[0]==0xFE) || //EZSP Payload
              (halHostResponseBuffer[0]==0xFD)) { //Bootloader Payload
      halHostResponseBuffer[halHostResponseBuffer[1] +1 +1] = FRAME_TERMINATOR;
      spipResponseLength = halHostResponseBuffer[1]+3;  //true Response length
    } else {
      halHostResponseBuffer[1] = FRAME_TERMINATOR;
      spipResponseLength = 2;  //true Response length
    }
    SCx_REG(TXENDA) = (uint32_t)halHostResponseBuffer + spipResponseLength - 1;
    SCx_REG(DATA) = 0xFF; // emlipari-183: Prepend sacrificial Tx pad byte
    INT_SCxFLAG = INT_SCRXVAL; //clear byte received interrupt
    SCx_REG(DMACTRL) = SC_TXLODA;   //enable response for TX
    INT_SCxCFG |= INT_SCRXVAL; //enable byte received interrupt
    CLR_nHOST_INT();           //tell the Host to get the response
    DEBUG_CLEAR_LED();
  } else { //no data to transmit, pump receive side
    //activate receive if not already and nSSEL is inactive
    if( ((SCx_REG(DMASTAT)&SC_RXACTA)!=SC_RXACTA) && nSSEL_IS_HIGH ) {
      volatile uint8_t dummy;
      //flush RX FIFO since the Wait and Response section overflowed it
      dummy = SCx_REG(DATA);
      dummy = SCx_REG(DATA);
      dummy = SCx_REG(DATA);
      dummy = SCx_REG(DATA);
      INT_SCxFLAG = INT_SCRXVAL; //clear byte received interrupt
      INT_SCxFLAG = INT_SCRXULDA;//clear buffer A unload interrupt
      INT_SCxCFG |= INT_SCRXVAL; //enable byte received interrupt
      INT_SCxCFG |= INT_SCRXULDA;//enable buffer A unload interrupt
      SCx_REG(DMACTRL) = SC_RXLODA; //we are inter-command, activate RX DMA for next
    }
    //check for valid start of data (counter!=0)
    //check for unloaded buffer
    if( (SCx_REG(RXCNTA)!=0) || (INT_SCxFLAG & INT_SCRXULDA) ) {
      spipFlagTransactionActive = true; //RX'ed, definitely in a transaction
      SET_nHOST_INT();  //by clocking a byte, the Host ack'ed nHOST_INT
      //if we have unloaded, know command arrived so jump directly there
      //bypassing RXCNT checks.  On em2xx this is needed because unload
      //clears RXCNT; on em3xx it is simply a convenience.
      if(INT_SCxFLAG & INT_SCRXULDA) {
        //While em2xx could get away with ACKing unload interrupt here,
        //because unload clears RXCNT, em3xx *must* do it below otherwise
        //a just-missed unload leaving RXCNT intact could mistakenly come
        //back to haunt us as a ghost command. -- BugzId:14622.
        goto dmaUnloadOnBootloaderFrame;
      }
      //we need at least 2 bytes before processing the Command
      if(SCx_REG(RXCNTA)>1) {
        //take action depending on the Command
        switch(halHostCommandBuffer[0]) {
          case 0x0A:
            processSpipCommandAndRespond(SPIP_VERSION);
            break;
          case 0x0B:
            processSpipCommandAndRespond(SPIP_ALIVE);
            break;
          case 0xFE: //The Command is an EZSP Frame
            //Fall into Bootloader Frame since processing the rest of the
            //command is the same. The only difference is responding with the
            //Unsupported SPI Command error
          case 0xFD: //The Command is a Bootloader Frame
            //guard against oversized messages which could cause serious problems
            if(halHostCommandBuffer[1] > MAX_PAYLOAD_FRAME_LENGTH) {
              wipeAndRestartSpi();
              setSpipErrorBuffer(SPIP_OVERSIZED_EZSP);
              return false; //dump!
            }
            //check for  all data before responding that we have a valid buffer
            if(SCx_REG(RXCNTA) >= halHostCommandBuffer[1]+3) {
dmaUnloadOnBootloaderFrame:
              DEBUG_SET_LED();//show me when stopped receiving
              INT_SCxCFG &= ~INT_SCRXVAL;//disable byte received interrupt
              INT_SCxCFG &= ~INT_SCRXULDA;//disable buffer A unload interrupt
              SCx_REG(DMACTRL) = SC_RXDMARST; //disable reception while processing
              INT_SCxFLAG = INT_SCRXULDA; //ack command unload --BugzId:14622
              DEBUG_CLEAR_LED();
              //check for Frame Terminator, it must be there!
              if(spipFlagOverrideResponse) {
                halHostSerialTick(true); //respond immediately!
                return false; //we overrode the command
              } else if(halHostCommandBuffer[halHostCommandBuffer[1]+2]
                        !=FRAME_TERMINATOR) {
                //no frame terminator found!  report missing F.T.
                setSpipErrorBuffer(SPIP_MISSING_FT);
                halHostSerialTick(true); //respond immediately!
                return false; //we overrode the command
              } else if(halHostCommandBuffer[0]==0xFE) {
                //load error response buffer with Unsupported SPI Command error
                setSpipErrorBuffer(SPIP_UNSUPPORTED_COMMAND);
                halHostSerialTick(true); //respond immediately!
                return false; //we overrode the command
              } else {
                halHostResponseBuffer[0] = 0xFD; //mark response Bootloader Frame
                return true; //there is a valid command
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
  //this fakes out SCx_ISR() in the normal SPI Protocol
  if(INT_SCxFLAG & INT_SCRXVAL) {
    SET_nHOST_INT(); //by clocking a byte, the Host ack'ed nHOST_INT
    spipFlagTransactionActive = true; //RX'ed, definitely in a transaction
  }
  
  //polling for the falling edge of the nWAKE
  //this fakes out halGpioIsr() in the normal SPI Protocol
  if(INT_GPIOFLAG & nWAKE_GPIOFLAG) {
    // ack int before read to avoid potential of missing interrupt
    INT_GPIOFLAG = nWAKE_GPIOFLAG;
    
    //A wakeup handshake should be performed in response to a falling edge on
    //the WAKE line. The handshake should only be performed on a SerialTick.
    spipFlagWakeFallingEdge = true;
  }
  
  //polling for the rising edge of nSSEL
  //this fakes out halTimerIsr() in the normal SPI Protocol
  //nSSEL signal comes in on IRQC
  if(INT_GPIOFLAG & nSSEL_GPIOFLAG) {
    // ack int before read to avoid potential of missing interrupt
    INT_GPIOFLAG = nSSEL_GPIOFLAG;
    
    //normally nHOST_INT is idled in the RXVALID Isr, but with short and fast
    //Responses, it's possible to service nSSEL before RXVALID, but we
    //still need to idle nHOST_INT.  If there is a pending callback, it will be
    //alerted via nHOST_INT at the end of this function.
    SET_nHOST_INT();
    
    //if we have not sent the exact right number of bytes, Transaction is corrupt
    if((SCx_REG(TXCNT) != spipResponseLength)) {
      setSpipErrorBuffer(SPIP_ABORTED_TRANSACTION);
    }
    
    //It's always safer to wipe the SPI clean and restart between transactions
    wipeAndRestartSpi();
    
    if(!spipFlagIdleHostInt) {
      CLR_nHOST_INT(); //we still have more to tell the Host
    }
  }
}

bool halHostSerialTick(bool responseReady)
{
  //Processing a potential premature nSSEL deactivation inside of the Tick
  //function will generate a lot of strange conditions that are best prevented
  //insteaded of handled.
  //Normal calls to halInternalHostSerialTick are <10us.  Worst case is <30us.
  //NOTE - The normal SPI Protocol wraps this call with an ATOMIC due to the
  //       above reason.  Since the bootloader does not use interrupts, we do
  //       not need to wrap this call in an ATOMIC.
  halGpioPolling();
  return halInternalHostSerialTick(responseReady);
}
