// *******************************************************************
// * bulb-ui.c
// *
// * Implements the user interface for the bulb.  If it is not on a network, 
// * it will first scan preferred channesl to join.  Then, it will scan all
// * channels until it joins.  If it is an end device and loses its parent, 
// * it will first scan the current and then preferred channels for a new
// * parent.  Eventually it will scan all channels every 30 minutes.
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/framework/plugin/bulb-pwm-configuration/bulb-config.h"
#include "app/framework/plugin-soc/manufacturing-library-cli/manufacturing-library-cli-plugin.h"

// ****************
// stack status typedefs.
#define EMBER_AF_BULB_UI_END_OF_RECORD 0xff
typedef struct {
  EmberNetworkStatus networkStatus;
  const uint8_t *name;
} EmberNetworkStates;

EmberNetworkStates emberNetworkStates[] = {
  { EMBER_NO_NETWORK,               "EMBER_NO_NETWORK" },
  { EMBER_JOINING_NETWORK,          "EMBER_JOINING_NETWORK" },
  { EMBER_JOINED_NETWORK,           "EMBER_JOINED_NETWORK" },
  { EMBER_JOINED_NETWORK_NO_PARENT, "EMBER_JOINED_NETWORK_NO_PARENT" },
  { EMBER_LEAVING_NETWORK,          "EMBER_LEAVING_NETWORK"},
  { EMBER_AF_BULB_UI_END_OF_RECORD, "EOR" }
};

typedef struct {
  EmberStatus stackStatus;
  const uint8_t *name;
} EmberStackStatus;

EmberStackStatus emberStackStatus[] = {
  { EMBER_NETWORK_UP,                "EMBER_NETWORK_UP"},
  { EMBER_NETWORK_DOWN,              "EMBER_NETWORK_DOWN"},
  { EMBER_JOIN_FAILED,               "EMBER_JOIN_FAILED"},
  { EMBER_MOVE_FAILED,               "EMBER_MOVE_FAILED"},
  { EMBER_CANNOT_JOIN_AS_ROUTER,     "EMBER_CANNOT_JOIN_AS_ROUTER"},
  { EMBER_NODE_ID_CHANGED,           "EMBER_NODE_ID_CHANGED"},
  { EMBER_PAN_ID_CHANGED,            "EMBER_PAN_ID_CHANGED"},
  { EMBER_CHANNEL_CHANGED,           "EMBER_CHANNEL_CHANGED"},
  { EMBER_NO_BEACONS,                "EMBER_NO_BEACONS"},
  { EMBER_RECEIVED_KEY_IN_THE_CLEAR, "EMBER_RECEIVED_KEY_IN_THE_CLEAR"},
  { EMBER_NO_NETWORK_KEY_RECEIVED,   "EMBER_NO_NETWORK_KEY_RECEIVED"},
  { EMBER_NO_LINK_KEY_RECEIVED,      "EMBER_NO_LINK_KEY_RECEIVED"},
  { EMBER_PRECONFIGURED_KEY_REQUIRED,"EMBER_PRECONFIGURED_KEY_REQUIRED"},
  { EMBER_AF_BULB_UI_END_OF_RECORD,  "EOR" }
};

// end of stack status typedefs.
// ****************

// forward declarations
static void emAfBulbUiLeftNetwork( void );
static void resetBindingsAndAttributes( void );
void emberAfGroupsClusterClearGroupTableCallback(uint8_t endpoint);
void emberAfScenesClusterClearSceneTableCallback(uint8_t endpoint);

// Framework declaratoins
EmberEventControl emberAfPluginBulbUiBulbUiRebootEventControl;
EmberEventControl emberAfPluginBulbUiRejoinEventControl;

#define rejoinEventControl   emberAfPluginBulbUiRejoinEventControl
#define REJOIN_TIME 30
#define BOOT_MONITOR_RESET_TIME 2  // number of seconds before the reboot 
                                   // monitor is reset.
static bool okToBlink = false;

static void startSearchForJoinableNetwork( void );

enum {
  REBOOT_EVENT_STATE_INITIAL_CHECK = 0,
  REBOOT_EVENT_STATE_WAIT_N_SECONDS = 1,
  REBOOT_EVENT_STATE_WAIT_10_SECONDS = 2
};

static uint8_t rebootEventState;

// Custom event stubs. Custom events will be run along with all other events in the
// application framework. They should be managed using the Ember Event API
// documented in stack/include/events.h

static void advanceRebootMonitor( void )
{
  tokTypeStackBootCounter rebootMonitor, rebootCounter;

  halCommonGetToken(&rebootCounter, TOKEN_STACK_BOOT_COUNTER);
  halCommonGetToken(&rebootMonitor, TOKEN_REBOOT_MONITOR);

  emberSerialPrintf(APP_SERIAL,"%2x %2x\r\n", rebootMonitor, rebootCounter);

  if( ((uint16_t) (rebootCounter - rebootMonitor)) > 30) {
    halCommonSetToken(TOKEN_REBOOT_MONITOR, &rebootCounter);
  } else while( ((uint16_t) (rebootCounter - rebootMonitor)) > 0) {
    halCommonIncrementCounterToken(TOKEN_REBOOT_MONITOR);
    rebootMonitor++;
    emberSerialPrintf(APP_SERIAL,"%2x %2x\r\n", rebootMonitor, rebootCounter);
  }
}

static void enableIdentify( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;

  uint8_t endpoint = fixedEndpoints[0];

  uint16_t identifyTime = (3*60); // three minutes

  emberAfWriteAttribute(endpoint,
                        ZCL_IDENTIFY_CLUSTER_ID,
                        ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t *) &identifyTime,
                        ZCL_INT16U_ATTRIBUTE_TYPE);
}

// Event function stub
void emberAfPluginBulbUiBulbUiRebootEventHandler(void) 
{
  tokTypeStackBootCounter rebootMonitor, rebootCounter;

  halCommonGetToken(&rebootCounter, TOKEN_STACK_BOOT_COUNTER);
  halCommonGetToken(&rebootMonitor, TOKEN_REBOOT_MONITOR);

  emberSerialPrintf(APP_SERIAL,"Reboot State: %d %d\r\n", rebootEventState, emberNetworkState());

  switch(rebootEventState) {
  case REBOOT_EVENT_STATE_INITIAL_CHECK:
    okToBlink = false;
    emberSerialPrintf(APP_SERIAL, "Bulb UI event Reset Reason:  %x %2x %p %p\r\n", 
      halGetResetInfo(),
      halGetExtendedResetInfo(),
      halGetResetString(),
      halGetExtendedResetString());

    if(emberNetworkState() == EMBER_NO_NETWORK) {
      advanceRebootMonitor();
      // don't kick off join if the mfglib is running.  
      if(emberAfMfglibEnabled()){
        emberEventControlSetDelayQS(emberAfPluginBulbUiBulbUiRebootEventControl, 40);
        rebootEventState = REBOOT_EVENT_STATE_WAIT_10_SECONDS;
        return;
      }

      startSearchForJoinableNetwork();
      emberEventControlSetInactive(emberAfPluginBulbUiBulbUiRebootEventControl);
      resetBindingsAndAttributes();
    } else {

      emberSerialPrintf(APP_SERIAL,"counters:  %2x %2x\r\n", rebootMonitor, rebootCounter );

      if(((uint16_t) (rebootCounter - rebootMonitor)) >= 10) {
        okToBlink = true;
        advanceRebootMonitor();
        emberLeaveNetwork();
        emberEventControlSetInactive(emberAfPluginBulbUiBulbUiRebootEventControl);
        //emberAfBasicClusterResetToFactoryDefaultsCallback();

      } else {
        rebootEventState = REBOOT_EVENT_STATE_WAIT_N_SECONDS;
        // set up the reboot event to run after 8 seconds
        emberEventControlSetDelayQS(emberAfPluginBulbUiBulbUiRebootEventControl, (BOOT_MONITOR_RESET_TIME*4));
      }
    }
      
    break;

  case REBOOT_EVENT_STATE_WAIT_N_SECONDS:
    emberSerialPrintf(APP_SERIAL, "Bulb UI wait N seconds, %x\r\n", ((uint16_t) (rebootCounter - rebootMonitor)));
    // depending on the number of reboots, we will want to take one of several actions
    if(((uint16_t) (rebootCounter - rebootMonitor)) >= 5) {
      // iControl wants us to rejoin here
      emberFindAndRejoinNetworkWithReason(false,  // unsecure rejoin 
                                          EMBER_ALL_802_15_4_CHANNELS_MASK, 
                                          EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE);
      emberSerialPrintf(APP_SERIAL, "Bulb UI Rejoin\r\n");

    } else if(((uint16_t) (rebootCounter - rebootMonitor)) == 4) {
      // turn on identify for 3 minutes
      emberSerialPrintf(APP_SERIAL, "Bulb UI Identify\r\n");
      enableIdentify();
    }
    advanceRebootMonitor();
    emberEventControlSetInactive(emberAfPluginBulbUiBulbUiRebootEventControl);
    break;
  case REBOOT_EVENT_STATE_WAIT_10_SECONDS:
    // we get here when the manufacturing lib token is enabled, and
    // the bulb has been on for 10 seconds.  
    emberEventControlSetInactive(emberAfPluginBulbUiBulbUiRebootEventControl);
    startSearchForJoinableNetwork();
    break;

  default:
    assert(0);
  }
}

uint8_t halGetResetInfo(void);

//void emberAfInitBulbUi( void )
void emberAfPluginBulbUiInitCallback( void )
{
  if(emberAfMfglibRunning())
    return;

  rebootEventState = REBOOT_EVENT_STATE_INITIAL_CHECK;
  //emberEventControlSetActive(emberAfPluginBulbUiBulbUiRebootEventControl);
  emberEventControlSetDelayMS(emberAfPluginBulbUiBulbUiRebootEventControl, 100);

  // print out reset reason.
  // this is test code for the eventual changing of bulb level and on/off 
  // settings based on a power on reset.  
  emberSerialPrintf(APP_SERIAL, "Bulb UI Reset Reason:  %x %2x %p %p\r\n", 
    halGetResetInfo(),
    halGetExtendedResetInfo(),
    halGetResetString(),
    halGetExtendedResetString());
}

// *****************join code

void emberAfPluginBulbUiFinishedCallback(EmberStatus status);

static uint8_t networkJoinAttempts = 0;

extern EmberStatus emberAfStartSearchForJoinableNetworkAllChannels(void);

static void startSearchForJoinableNetwork( void )
{
  if(emberAfMfglibRunning()) 
    return;

  okToBlink = true;
  if(networkJoinAttempts < 20) {
    networkJoinAttempts++;
    if(networkJoinAttempts == 1) {
      emberAfStartSearchForJoinableNetwork();
    } else {
      emberAfStartSearchForJoinableNetworkAllChannels();
    }
    // call the event in 20 seconds in case we don't get the callback.
    emberEventControlSetDelayQS(rejoinEventControl, (20*4) );
  } else {
    emberAfPluginBulbUiFinishedCallback(EMBER_NOT_JOINED);
  }
}

static void resetJoinAttempts( void )
{
  networkJoinAttempts = 0;
}

// stack status routines
static void printNetworkState( void )
{
  uint8_t i = 0;
  EmberNetworkStatus currentNetworkStatus = emberNetworkState();

  while(emberNetworkStates[i].networkStatus != EMBER_AF_BULB_UI_END_OF_RECORD) {
    if(emberNetworkStates[i].networkStatus == currentNetworkStatus) {
      emberSerialPrintf(APP_SERIAL,"%p ", 
                        emberNetworkStates[i].name);
      return;
    }
    i++;
  }
}

static void printNetworkStatus( EmberStatus status )
{
  uint8_t i = 0;

  while(emberStackStatus[i].stackStatus != EMBER_AF_BULB_UI_END_OF_RECORD) {
    if(emberStackStatus[i].stackStatus == status) {
      emberSerialPrintf(APP_SERIAL,"%p", 
                        emberStackStatus[i].name);
      return;
    }
    i++;
  }
  emberSerialPrintf(APP_SERIAL,"unknown %x",status);
}

void emberAfPluginBulbUiStackStatusCallback( EmberStatus status )
{
  emberSerialPrintf(APP_SERIAL,"Stack Status Handler:  ");

  printNetworkState();
  printNetworkStatus( status );

  switch(status) {
  case EMBER_NETWORK_UP:
    resetJoinAttempts();
    if(okToBlink) {
      emberAfPluginBulbConfigLedBlink(10, 100);
      okToBlink = false;
    }
    break;
  case EMBER_NETWORK_DOWN:
    break;
  case EMBER_JOIN_FAILED:
    break;
  case EMBER_MOVE_FAILED:
    break;
  case EMBER_CANNOT_JOIN_AS_ROUTER:
    break;
  case EMBER_NODE_ID_CHANGED:
    break;
  case EMBER_PAN_ID_CHANGED:
    break;
  case EMBER_CHANNEL_CHANGED:
    break;
  case EMBER_NO_BEACONS:
    break;
  case EMBER_RECEIVED_KEY_IN_THE_CLEAR:
    break;
  case EMBER_NO_NETWORK_KEY_RECEIVED:
    break;
  case EMBER_NO_LINK_KEY_RECEIVED:
    break;
  case EMBER_PRECONFIGURED_KEY_REQUIRED:
    break;
  default:
    break;
  }
  emberSerialPrintf(APP_SERIAL,"\r\n");


  if(status == EMBER_NETWORK_DOWN && 
     emberNetworkState() == EMBER_NO_NETWORK) {
    emberSerialPrintf(APP_SERIAL,"BulbUi: search for joinable network\r\n");

    emAfBulbUiLeftNetwork();

    startSearchForJoinableNetwork();
  }
  
  if(status == EMBER_NETWORK_DOWN && 
     emberNetworkState() == EMBER_JOINED_NETWORK_NO_PARENT) {
    emberSerialPrintf(APP_SERIAL,"BulbUi: kick off rejoin event in %d minutes.\r\n", REJOIN_TIME);
    emberEventControlSetDelayQS(rejoinEventControl, (REJOIN_TIME*4*60) );
  }
}

// rejoin code
void emberAfPluginBulbUiRejoinEventHandler(void)
{
  emberEventControlSetInactive(rejoinEventControl);

  emberSerialPrintf(APP_SERIAL,"Rejoin event function ");
  printNetworkState();
  
  switch(emberNetworkState()) {
  case EMBER_NO_NETWORK:
    startSearchForJoinableNetwork();
    break;
  case EMBER_JOINED_NETWORK_NO_PARENT:
    // in case the bulb is a sleep end device
    // perform the secure rejoin every 30 minutes until we find a network.
    emberSerialPrintf(APP_SERIAL,"Perform and schedule rejoin\r\n");
    emberEventControlSetDelayQS(rejoinEventControl, (REJOIN_TIME*4*60) );
    emberFindAndRejoinNetworkWithReason(false,  // unsecure rejoin 
                                        EMBER_ALL_802_15_4_CHANNELS_MASK, 
                                        EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE);
    break;
  case EMBER_JOINING_NETWORK:
    break;
  default:
    emberSerialPrintf(APP_SERIAL,"No More Rejoin!\r\n");
    break;
  }
}

/** @brief Finished
 *
 * This callback is fired when the network-find plugin is finished with the
 * forming or joining process.  The result of the operation will be returned
 * in the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginNetworkFindFinishedCallback(EmberStatus status)
{
  emberSerialPrintf(APP_SERIAL,"BULB_UI:  Network Find status %x\r\n", status);
  emberAfPluginBulbUiFinishedCallback(status);

  if(status == EMBER_SUCCESS) {
    emberEventControlSetInactive(rejoinEventControl);
  } else {
    // delay the rejoin for 5 seconds.  
    emberEventControlSetDelayQS(rejoinEventControl, (5*4));
  }
}

static void resetBindingsAndAttributes( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;

  uint8_t endpoint = fixedEndpoints[0];
  uint8_t i;

  for(i=0; i< FIXED_ENDPOINT_COUNT; i++) {
    endpoint = fixedEndpoints[i];

    // first restore defaults.
    emberAfResetAttributes( endpoint );

    // now, clear the binding and group table
    // per spec 8.3.3 EZ Mode Commissioning
    emberAfGroupsClusterClearGroupTableCallback(endpoint);
    emberAfScenesClusterClearSceneTableCallback(endpoint); 
  }
}

// here is the list of things we need to do whenever we have left the netowrk.
static void emAfBulbUiLeftNetwork( void )
{
  resetBindingsAndAttributes();

  // now, blink the LED 3 times
  emberAfPluginBulbConfigLedBlink(3, 500);
}
