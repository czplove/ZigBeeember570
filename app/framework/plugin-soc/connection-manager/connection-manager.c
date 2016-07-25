// *******************************************************************
// * connection-manager.c
// *
// * Implements code to maintain a network connection.  It will implement rejoin
// * algorithms and perform activity LED blinking as required.
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *******************************************************************

#ifdef EMBER_SCRIPTED_TEST
#include "app/framework/plugin-soc/connection-manager/connection-manager-test.h"
#endif

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/framework/plugin-soc/manufacturing-library-cli/manufacturing-library-cli-plugin.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "hal/micro/led-blink.h"

//------------------------------------------------------------------------------
// Plugin private macro definitions
#define REJOIN_TIME_MINUTES   EMBER_AF_PLUGIN_CONNECTION_MANAGER_REJOIN_TIME_M
#define REJOIN_FAILED_RETRY_TIME_SECONDS \
  EMBER_AF_PLUGIN_CONNECTION_MANAGER_RETRY_TIME_S
#define REJOIN_FAILED_RETRY_TIME_QS \
  REJOIN_FAILED_RETRY_TIME_SECONDS * 4
#define REJOIN_ATTEMPTS       EMBER_AF_PLUGIN_CONNECTION_MANAGER_REJOIN_ATTEMPTS

// These are default values to modify the UI LED's blink pattern for network
// join and network leave.
#define LED_LOST_ON_TIME_MS          250
#define LED_LOST_OFF_TIME_MS         750
#define LED_BLINK_ON_TIME_MS         200
#define LED_SEARCH_BLINK_OFF_TIME_MS 1800
#define LED_FOUND_BLINK_OFF_TIME_MS  250
#define LED_FOUND_BLINK_ON_TIME_MS   250
#define LED_IDENTIFY_ON_TIME_MS      250
#define LED_IDENTIFY_OFF1_TIME_MS    250
#define LED_IDENTIFY_OFF2_TIME_MS    1250
#define DEFAULT_NUM_JOIN_BLINKS      6
#define DEFAULT_NUM_LEAVE_BLINKS     3
#define DEFAULT_NUM_SEARCH_BLINKS    100
#define DEFAULT_NUM_IDENTIFY_BLINKS  100

#define SECONDS_BETWEEN_JOIN_ATTEMPTS 20
#define QS_BETWEEN_JOIN_ATTEMPTS      (SECONDS_BETWEEN_JOIN_ATTEMPTS * 4)
#define NON_SLEEPY_POLL_INTERVAL_S    1
#define NON_SLEEPY_POLL_INTERVAL_QS   (NON_SLEEPY_POLL_INTERVAL_S * 4)

#define HA_SLEEPY_DEVICE_POLL_LENGTH_S    60
#define HA_SLEEPY_DEVICE_POLL_LENGTH_MS   (1000*HA_SLEEPY_DEVICE_POLL_LENGTH_S)

#ifdef EMBER_AF_HAS_SLEEPY_NETWORK
#define FORCE_SHORT_POLL() \
  emberAfAddToCurrentAppTasks(EMBER_AF_FORCE_SHORT_POLL)
#define UNFORCE_SHORT_POLL() \
  emberAfRemoveFromCurrentAppTasks(EMBER_AF_FORCE_SHORT_POLL)
#else
#define FORCE_SHORT_POLL()
#define UNFORCE_SHORT_POLL()
#endif

typedef struct {
  EmberStatus status;
  const char * const message;
} NetworkStateMessage;

typedef struct {
  EmberNetworkStatus status;
  const char * const message;
} EmberStatusMessage;

//------------------------------------------------------------------------------
// Plugin events
EmberEventControl emberAfPluginConnectionManagerRebootEventControl;
EmberEventControl emberAfPluginConnectionManagerRejoinEventControl;
EmberEventControl emberAfPluginConnectionManagerPollEventControl;

//------------------------------------------------------------------------------
// plugin private global variables

static NetworkStateMessage networkStateMessages[] =
{
  {EMBER_NETWORK_UP,                "EMBER_NETWORK_UP"},
  {EMBER_NETWORK_DOWN,              "EMBER_NETWORK_DOWN"},
  {EMBER_CHANNEL_CHANGED,           "EMBER_CHANNEL_CHANGED"},
  {EMBER_JOIN_FAILED,               "EMBER_JOIN_FAILED"},
  {EMBER_NO_NETWORK_KEY_RECEIVED,   "EMBER_NO_NETWORK_KEY_RECEIVED"},
};

static EmberStatusMessage emberStatusMessages[] =
{
  {EMBER_NO_NETWORK,                "EMBER_NO_NETWORK"},
  {EMBER_JOINING_NETWORK,           "EMBER_JOINING_NETWORK"},
  {EMBER_JOINED_NETWORK,            "EMBER_JOINED_NETWORK"},
  {EMBER_JOINED_NETWORK_NO_PARENT,  "EMBER_JOINED_NETWORK_NO_PARENT"},
  {EMBER_LEAVING_NETWORK,           "EMBER_LEAVING_NETWORK"},
};

// State variables for controlling LED blink behavior on network join/leave
static uint16_t networkLostBlinkPattern[] = {
  LED_LOST_ON_TIME_MS, LED_LOST_OFF_TIME_MS};
static uint16_t networkSearchBlinkPattern[] = 
  {LED_BLINK_ON_TIME_MS, LED_SEARCH_BLINK_OFF_TIME_MS};
static uint16_t networkFoundBlinkPattern[] =
  {LED_FOUND_BLINK_ON_TIME_MS, LED_FOUND_BLINK_OFF_TIME_MS};
static uint16_t networkIdentifyBlinkPattern[] =
{
  LED_IDENTIFY_ON_TIME_MS, 
  LED_IDENTIFY_OFF1_TIME_MS, 
  LED_IDENTIFY_ON_TIME_MS,
  LED_IDENTIFY_OFF2_TIME_MS
};
  
static uint8_t numJoinBlinks = DEFAULT_NUM_JOIN_BLINKS;
static uint8_t numLeaveBlinks = DEFAULT_NUM_LEAVE_BLINKS;
static uint8_t numSearchBlinks = DEFAULT_NUM_SEARCH_BLINKS;
static uint8_t numIdentifyBlinks = DEFAULT_NUM_IDENTIFY_BLINKS;

// Track number of times plugin has attempted to join a network
static uint8_t networkJoinAttempts = 0;

//------------------------------------------------------------------------------
// plugin private function prototypes

void emberAfPluginConnectionManagerLedNetworkFoundBlink(void);

static void ledNetworkFoundBlink(void);
static void ledNetworkLostBlink(void);
static void ledNetworkSearchBlink(void);
static void ledNetworkIdentifyBlink(void);
static void printNetworkState(EmberNetworkStatus emStatus);
static void printStackStatus(EmberStatus status);
static void clearNetworkTables(void);

// forward declaration of plugin callbacks
void emberAfPluginConnectionManagerFinishedCallback(EmberStatus status);

//------------------------------------------------------------------------------
// extern'd functions and global variables from other files
extern EmberStatus emberAfStartSearchForJoinableNetworkAllChannels(void);

//------------------------------------------------------------------------------
// Plugin consumed callback implementations

//******************************************************************************
// Init callback, executes sometime early in device boot chain.  This function
// will handle debug and UI LED control on startup, and schedule the reboot
// event to occur after the system finishes initializing.
//******************************************************************************
void emberAfPluginConnectionManagerInitCallback(void)
{
  emberEventControlSetActive(emberAfPluginConnectionManagerRebootEventControl);
}

//******************************************************************************
// This callback is fired when the network-find plugin is finished with the
// forming or joining process.  The result of the operation will be returned
// in the status parameter.
//
// param status:   Ver.: always
//******************************************************************************
void emberAfPluginNetworkFindFinishedCallback(EmberStatus status)
{
  emberAfAppPrintln("Connection Manager:  Network Find status %x",
                    status);
  emberAfPluginConnectionManagerFinishedCallback(status);

  if (status == EMBER_SUCCESS) {
    emberEventControlSetInactive(emberAfPluginConnectionManagerRejoinEventControl);
  } else {
    // delay the rejoin for 5 seconds.
    emberEventControlSetDelayQS(emberAfPluginConnectionManagerRejoinEventControl,
                                (REJOIN_FAILED_RETRY_TIME_QS));
  }
}

//******************************************************************************
// This callback will execute whenever the network stack has a significant
// change in state (network joined, network join failed, etc).  It is mostly
// used for debug and UI LED control.
//******************************************************************************
void emberAfPluginConnectionManagerStackStatusCallback(EmberStatus status)
{  
  emberAfAppPrint("Stack Status Handler:  ");

  printNetworkState(emberAfNetworkState());
  printStackStatus(status);

  if (status == EMBER_NETWORK_UP) {
    emberAfPluginConnectionManagerResetJoinAttempts();
    ledNetworkFoundBlink();
    
    // After a successful join, a sleepy end device must actively respond to
    // communication from the network in order to not be marked as an
    // unresponsive device.  This is facilitated by manually performing a poll
    // once per second for a minute, which is done with the PollEvent.
    FORCE_SHORT_POLL();
    emberEventControlSetDelayMS(emberAfPluginConnectionManagerPollEventControl,
                                HA_SLEEPY_DEVICE_POLL_LENGTH_MS);
  } else if (status == EMBER_NETWORK_DOWN
             && emberAfNetworkState() == EMBER_NO_NETWORK) {
    // If the network goes down, the device needs to clear its binding table
    // (as a new one will be generated by the network when it comes back up),
    // and clear any groups or scenes the network has set.
    emberAfAppPrintln( "Connection Manager: search for joinable network");
    clearNetworkTables();
    ledNetworkLostBlink();
    networkJoinAttempts = 0;
    emberAfPluginConnectionManagerStartSearchForJoinableNetwork();
  } else if (status == EMBER_NETWORK_DOWN
             && emberAfNetworkState() == EMBER_JOINED_NETWORK_NO_PARENT) {
    emberAfAppPrintln("Connection Manager: kick off rejoin event.");
    emberEventControlSetDelayMinutes(
      emberAfPluginConnectionManagerRejoinEventControl,
      REJOIN_TIME_MINUTES);
    halLedBlinkLedOff(0);
  }
}

//------------------------------------------------------------------------------
// Plugin event handlers

//******************************************************************************
// Reboot event.  To be called sometime after all system init functions have
// executed.  This function will check the network state, and initiate a search
// for new networks to join if the device is not currently on a network.
//******************************************************************************
void emberAfPluginConnectionManagerRebootEventHandler(void)
{
  uint8_t shortPollForced;

  halCommonGetToken(&shortPollForced, TOKEN_FORCE_SHORT_POLL);
  
  if (shortPollForced) {
    emberAfAppPrint("Short poll forced to permanently enabled.");
    FORCE_SHORT_POLL();
  } else {
    UNFORCE_SHORT_POLL();
  }
  
  emberEventControlSetInactive(emberAfPluginConnectionManagerRebootEventControl);

  if (emberAfNetworkState() == EMBER_NO_NETWORK) {
    ledNetworkLostBlink();
    emberAfPluginConnectionManagerStartSearchForJoinableNetwork();
  }
  else if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    ledNetworkFoundBlink();
  }
}

//******************************************************************************
// Fast Poll event handler.  This will cause the (sleepy) device to poll the
// network for pending data every second for a minute after joining the
// newtork.  This is necessary to service messages from the gatweay in a
// reasonable amount of time (one sec delay as opposed to 30 second delay)
//******************************************************************************
void emberAfPluginConnectionManagerPollEventHandler(void)
{
  uint8_t shortPollForced;
  emberEventControlSetInactive(emberAfPluginConnectionManagerPollEventControl);
  
  halCommonGetToken(&shortPollForced, TOKEN_FORCE_SHORT_POLL);
  
  // We should only remove ourselves from the short poll task if one minute
  // has expired since network join AND the token to force short poll is set to
  // disable.
  if (shortPollForced) {
    emberAfAppPrint("Short poll forced to permanently enabled.");
  } else {
    UNFORCE_SHORT_POLL();
  }
}

//******************************************************************************
// Rejoin Event.  This event is used to attempt a network rejoin and to verify
// that the parent node has not died.
//******************************************************************************
void emberAfPluginConnectionManagerRejoinEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginConnectionManagerRejoinEventControl);

  emberAfAppPrint("Rejoin event function ");
  printNetworkState(emberAfNetworkState());

  switch(emberAfNetworkState()) {
  case EMBER_NO_NETWORK:
    emberAfPluginConnectionManagerStartSearchForJoinableNetwork();
    break;
  case EMBER_JOINED_NETWORK_NO_PARENT:
    // since the sensor is a sleepy end device, perform the secure rejoing
    // every 30 minutes until we find a network.
    emberAfAppPrintln("Perform and schedule rejoin");
    emberEventControlSetDelayMinutes(
      emberAfPluginConnectionManagerRejoinEventControl,
      REJOIN_TIME_MINUTES);
    emberAfStartMoveCallback();
    break;
  case EMBER_JOINING_NETWORK:
    break;
  default:
    emberAfAppPrintln("No More Rejoin!");
    break;
  }
}

//------------------------------------------------------------------------------
// Plugin public API function implementations

// *****************************************************************************
// Blink network activity LED to notify user of network lost state
// If this is the first search, search only on preferred channels
//   Otherwise, search on all channels
// Make sure another attempt occurs in 20 seconds until 20 failures are seen
// If more than 20 network join attempts fail, inform user via callback
// *****************************************************************************
void emberAfPluginConnectionManagerStartSearchForJoinableNetwork(void)
{
  if (emberAfMfglibRunning() || emberAfMfglibEnabled()) 
    return;
  if (networkJoinAttempts < REJOIN_ATTEMPTS) {
    networkJoinAttempts++;
    if (networkJoinAttempts == 1) {
      emberAfStartSearchForJoinableNetwork();
    } else {
      ledNetworkSearchBlink();
      emberAfStartSearchForJoinableNetworkAllChannels();
    }
    // call the event in 20 seconds in case we don't get the stack status 
    // callback (which will happen if there's no network to join)
    emberEventControlSetDelayQS(emberAfPluginConnectionManagerRejoinEventControl,
                                QS_BETWEEN_JOIN_ATTEMPTS);
  } else {
    emberAfAppPrintln("Failed to find network to join within %d attempts",
                      networkJoinAttempts);
    emberAfPluginConnectionManagerFinishedCallback(EMBER_NOT_JOINED);
    halLedBlinkLedOff(0);
  }
}

// *****************************************************************************
// Reset network join attempts to zero
// *****************************************************************************
void emberAfPluginConnectionManagerResetJoinAttempts(void)
{
  networkJoinAttempts = 0;
}

// *****************************************************************************
// Print current state of the network, leave the network, and start searching
// for a new network.
// *****************************************************************************
void emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne(void)
{
  printNetworkState(emberAfNetworkState());
  networkJoinAttempts = 0;
  if ((emberAfNetworkState() == EMBER_JOINED_NETWORK)
      || (emberAfNetworkState() == EMBER_JOINED_NETWORK_NO_PARENT)) {
    ledNetworkLostBlink();
    emberLeaveNetwork();
  } else {
    emberAfPluginConnectionManagerStartSearchForJoinableNetwork();
    ledNetworkSearchBlink();
  }
}

static void clearNetworkTables(void)
{
  uint8_t ep;
  uint8_t endpoint;
  
  emberClearBindingTable();
  emberAfClearReportTableCallback();
  for (ep = 0; ep < emberAfEndpointCount(); ep++) {
    endpoint = emberAfEndpointFromIndex(ep);
    emberAfResetAttributes(endpoint);
    emberAfGroupsClusterClearGroupTableCallback(endpoint);
    emberAfScenesClusterClearSceneTableCallback(endpoint);
  }
}

// *****************************************************************************
// Reset all the endpoints, clear all binding, scene, and group tables, leave
// the network, and start searching for a new one.
// *****************************************************************************
void emberAfPluginConnectionManagerFactoryReset(void)
{
  clearNetworkTables();
}

// *****************************************************************************
// Update private state variable for join blinks
// *****************************************************************************
void emberAfPluginConnectionManagerSetNumberJoinBlink(uint8_t numBlinks)
{
  numJoinBlinks = numBlinks;
}

// *****************************************************************************
// Update private state variable for leave blinks
// *****************************************************************************
void emberAfPluginConnectionManagerSetNumberLeaveBlink(uint8_t numBlinks)
{
  numLeaveBlinks = numBlinks;
}

//------------------------------------------------------------------------------
// Plugin private function implementations

static void printNetworkState(EmberNetworkStatus emStatus)
{
  uint8_t i;
  
  for (i = 0; i < COUNTOF(emberStatusMessages); i++) {
    if (emStatus == emberStatusMessages[i].status) {
      emberAfAppPrint("%s ", emberStatusMessages[i].message);
      return;
    }
  }
}

static void printStackStatus(EmberStatus emStatus)
{
  uint8_t i;
  
  for (i = 0; i < COUNTOF(emberStatusMessages); i++) {
    if (emStatus == networkStateMessages[i].status) {
      emberAfAppPrint("%s ", networkStateMessages[i].message);
      return;
    }
  }
}

void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{
  ledNetworkIdentifyBlink();
}

void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
  halLedBlinkLedOff(0);
}

void emberAfPluginConnectionManagerLedNetworkFoundBlink(void)
{
  ledNetworkFoundBlink();
}

static void ledNetworkIdentifyBlink(void)
{
  halLedBlinkPattern(numIdentifyBlinks, 4, networkIdentifyBlinkPattern);
}

static void ledNetworkFoundBlink(void)
{
  halLedBlinkPattern(numJoinBlinks, 2, networkFoundBlinkPattern);
}

static void ledNetworkLostBlink(void)
{
  halLedBlinkPattern(numLeaveBlinks, 2, networkLostBlinkPattern);
}

static void ledNetworkSearchBlink(void)
{
  halLedBlinkPattern(numSearchBlinks, 2, networkSearchBlinkPattern);
}
