// *******************************************************************
// * HaCapSenseDimmerSwitchCallbacks.c
// *
// * This file implements all state machine and system callbacks required to
// * us an EF8 "sleepy bee" based capacitive touch gesture sensor as a switch
// * to control brightness, color temperature, and color hue of a zigbee light
// * bulb.
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.          *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "hal/micro/button-interface.h"
#include "hal/micro/i2c-driver.h"
#include "hal/micro/sb1-gesture-sensor.h"
#include "hal/micro/led-blink.h"

//------------------------------------------------------------------------------
// Application specific macros

// Macros used to enable/disable application behavior
#ifndef COLOR_CHANGE_ENABLED
#define COLOR_CHANGE_ENABLED            1
#endif

#ifndef LED_ENABLED
#define LED_ENABLED                     1
#endif

#ifndef SWIPING_ENABLED
#define SWIPING_ENABLED                 1
#endif

// Macros used to determine if switch is controlling Hue, Temp, or brightness
#define NORMAL_TAB 0
#define TEMP_TAB   1
#define HUE_TAB    2

// Macros used in color temperature and hue stepping commands
#define COLOR_TEMP_STEP_POSITIVE        1
#define COLOR_TEMP_STEP_NEGATIVE        3
#define COLOR_TEMP_STEP_AMOUNT          50 
#define COLOR_HUE_STEP_POSITIVE         1
#define COLOR_HUE_STEP_NEGATIVE         3
#define COLOR_HUE_STEP_AMOUNT           10

// Button interface macros
// Debounce time for the button.  Any presses shorter than this will be ignored.
#define NETWORK_BUTTON_DEBOUNCE_TIME_MS 50
// Time between presses.  This is the maximum amount of time that can pass
// between a button being pressed for two presses to be considered consecutive.
// Note that this time is the period between the end of the button presses.
#define NETWORK_BUTTON_MAX_TIME_BETWEEN_PRESSES_MS  \
                                        500

// Endpoint macros
// The endpoint on the dimmer switch that is a client of the lighting based
// clusters, which will be used as the source endpoint when sending out
// lighting commands.  This is the index into the endpoint list, not necessarily
// the actual endpoint.
#define LIGHTING_CLIENT_ENDPOINT_INDEX  0

//------------------------------------------------------------------------------
// Application specific structure and enum declarations

//switch command protocol from cap sense. each command is passed as an integer
enum {
  SWITCH_PROTOCOL_OFF             = 0x01,
  SWITCH_PROTOCOL_ON              = 0x02,
  SWITCH_PROTOCOL_LEVEL_UP        = 0x03,
  SWITCH_PROTOCOL_LEVEL_DN        = 0x04,
  SWITCH_PROTOCOL_SWIPE_L         = 0x05,
  SWITCH_PROTOCOL_SWIPE_R         = 0x06,
  SWITCH_PROTOCOL_NONE            = 0x00,
  SWITCH_PROTOCOL_NOT_READY       = 0xFF
};

//------------------------------------------------------------------------------
// Event function forward declaration
EmberEventControl frameTimeoutControl;
EmberEventControl buttonPressCountEventControl;
EmberEventControl sendMatchDescriptorEventControl;

static void formCommandFromHoldUp();
static void formCommandFromHoldDown();
static void localSendCommand(void);
static void enableIdentify(void);
static void generateMatchDescriptorRequest(EmberAfClusterId clusterId);
static void matchDescriptorCallback(
  const EmberAfServiceDiscoveryResult* result);
static EmberStatus addToBindingTable(EmberBindingTableEntry *entry);

//------------------------------------------------------------------------------
// External function declarations
extern void zclBufferSetup(int8u frameType, int16u clusterId, int8u commandId);
extern void zclBufferAddByte(int8u byte);
extern void zclBufferAddWord(int16u word);
extern void zclBufferAddWordFromArgument(int8u index);
extern void emAfApsFrameEndpointSetup(int8u srcEndpoint, int8u dstEndpoint);

//------------------------------------------------------------------------------
// External global variable declarations.
extern EmberApsFrame globalApsFrame;
extern int8u appZclBuffer[];
extern int16u appZclBufferLen;
extern int16u mfgSpecificId;
extern boolean zclCmdIsBuilt;

//------------------------------------------------------------------------------
// Application specific global variables

// State variable for what tab is currently being controlled
static int8u tab_state = NORMAL_TAB;

// State variables for providing a smooth transition between different
// brightness levels when dimming light
static int8u dimStepSize = 30;
static int16u dimTime = 3;

// Number of consecutive button presses received so far
static int8u consecutiveButtonPressCount = 0;

// This array is used to track all clusters the switch can use to send lighting
// commands.  It is used to set up binding table entries so that any on/off,
// level control, and color control commands that are generated as a result of
// user interaction with the switch will be sent by default to the gateway.  If
// the expected behavior is that the gateway will handle setting up the binding
// table, this variable can be removed, along with the calls to
// generateMatchDescriptorRequest, which is the mechanism by which the binding
// table entries are created.
static int16u lightingClientClusters[] = 
{
  ZCL_ON_OFF_CLUSTER_ID,
  ZCL_LEVEL_CONTROL_CLUSTER_ID,
  ZCL_COLOR_CONTROL_CLUSTER_ID,
};

static int8u lightingClientClusterIdx = 0;

//------------------------------------------------------------------------------
// Implemented functions

//------------------------------------------------------------------------------
// Framework stack status callback
//

// This function is called by the application framework from the stack status
// handler.  When the switch joins a network, it needs to send out a series of
// match descriptor requests to determine where it should send messages to when
// it receives capacitive sense button presses.
boolean emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP){
    lightingClientClusterIdx = 0;
    generateMatchDescriptorRequest(lightingClientClusters[0]);
  }
  return TRUE;
}

//------------------------------------------------------------------------------
// This will create a match descriptor request, which will cause the coordinator
// to reply with enough information to create a binding table entry for all
// devices on the network that are listening for messages that the dimmer switch
// is expected to provide.
//static void generateMatchDescriptorRequest(void)
static void generateMatchDescriptorRequest(EmberAfClusterId clusterId)
{
  EmberNodeId target;
  EmberAfProfileId profile;
  boolean serverCluster;

  // Target the co-ordinator, which should always be the gateway
  target = EMBER_ZIGBEE_COORDINATOR_ADDRESS;

  // profile: Home Automation.
  profile = HA_PROFILE_ID;

  // cluster ID is based on the list of lighting client clusters.
  clusterId = lightingClientClusters[lightingClientClusterIdx];
  emberAfAppPrintln("match descriptor request on cluster 0x%x", clusterId);

  // serverCluster: client, because the gateway will be consuming the on-off
  // commands.
  serverCluster = EMBER_AF_CLIENT_CLUSTER_DISCOVERY;

  emberAfFindDevicesByProfileAndCluster(target,
                                        profile,
                                        clusterId,
                                        serverCluster,
                                        matchDescriptorCallback);
}

//------------------------------------------------------------------------------
// This function is executed when the device receives a response to a
// findDevicesByProfileAndCluster query.  It will parse the resposne and create
// a binding table entry for all devices on the network that are listening for
// messages that the dimmer switch will provide.
static void matchDescriptorCallback(const EmberAfServiceDiscoveryResult* result)
{
  EmberEUI64 destEUI;
  EmberBindingTableEntry binding_entry;
  int8u srcEndpoint;
  int8u destEndpoint;
  EmberStatus status;
  const EmberAfEndpointList* epList;
 
  // Find a valid response, then set the destination endpoint to the endpoint
  // given by the response and create a binding table entry.
  if (!emberAfHaveDiscoveryResponseStatus(result->status)) {
    return;
  } else if (result->zdoRequestClusterId == MATCH_DESCRIPTORS_REQUEST) {
    // Get information from result required to create the binding table entry
    emberLookupEui64ByNodeId(result->matchAddress, destEUI);
    epList = (const EmberAfEndpointList*)result->responseData;
    srcEndpoint = emberAfEndpointFromIndex(LIGHTING_CLIENT_ENDPOINT_INDEX);
    
    // Currently, this only supports a gateway with a single endpoint
    destEndpoint= epList->list[0];
    emberAfAppPrintln("set coordinator endpoint to %d", destEndpoint);
    
    // set binding to this endpoint/address/cluster
    binding_entry.type = EMBER_UNICAST_BINDING;
    binding_entry.local = srcEndpoint;
    binding_entry.remote = destEndpoint;
    MEMCOPY(binding_entry.identifier, destEUI, EUI64_SIZE);

    // cycle through the list of lighting clusters that can be used to send
    // commands from the switch, adding a binding table entry for each.    
    binding_entry.clusterId = lightingClientClusters[lightingClientClusterIdx];
    status = addToBindingTable(&binding_entry);
    
    if (status != EMBER_SUCCESS) {
      emberAfAppPrintln("Error adding binding entry: 0x%x", status);
    }
  }
  
  // Generate the next match descriptor request.  This must be done in a later
  // activated event (as opposed to a direct call to 
  // generateMatchDescriptorRequest here), since the framework still has
  // processing left to do after matchDescriptorCallback returns to close out
  // the last match descriptor response before a new request can be generated.
  emberEventControlSetActive(sendMatchDescriptorEventControl);
}

void sendMatchDescriptorEventHandler(void)
{
  emberEventControlSetInactive(sendMatchDescriptorEventControl);
  
  lightingClientClusterIdx++;
  if (lightingClientClusterIdx < COUNTOF(lightingClientClusters)) {
    emberAfAppPrintln("creating match descriptor request %d", lightingClientClusterIdx);
    generateMatchDescriptorRequest(
      lightingClientClusters[lightingClientClusterIdx]);
  }
}


//------------------------------------------------------------------------------
// This function will find the lowest indexed entry of the binding table that is
// not currently in use and add the given entry to that location.
static EmberStatus addToBindingTable(EmberBindingTableEntry *entry)
{
  EmberStatus status;
  
  //for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++)
  //{
  //  if (!emberBindingIsActive(i)) {
      emberAfAppPrintln("Adding entry to idx %d", lightingClientClusterIdx);
      status = emberSetBinding(lightingClientClusterIdx, entry);
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("Error setting binding table entry: 0x%x", status);
      }
      emberSetBindingRemoteNodeId(lightingClientClusterIdx, 0);
  //    return status;
  //  }
  //}
  
  // if all binding indeces are used, return an error
  return status;
}

//------------------------------------------------------------------------------
// Button0PressedShortCallback
// This should be called when a short press is detected on the network join
// pushbutton.  Actions based on short presses only occur if the button is
// pressed multiple times, so all that will be done here is to increment the
// button press counter and delay the "no more short presses" events'
// activation again.  Once that event triggers, the number of presses will be
// evaluated, and action will be taken based on that number of presses.
void emberAfPluginButtonInterfaceButton0PressedShortCallback(
  int16u timePressedMs)
{
  
  // If the button was not held for longer than the debounce time, ignore the
  // press.
  if (timePressedMs < NETWORK_BUTTON_DEBOUNCE_TIME_MS) {
    return;
  }

  consecutiveButtonPressCount++;

  emberEventControlSetDelayMS(buttonPressCountEventControl,
                              NETWORK_BUTTON_MAX_TIME_BETWEEN_PRESSES_MS);
}

//------------------------------------------------------------------------------
// Button0PressedLongCallback
// This should be called when a long press is detected on the network join
// pushbutton.  It should cause the device to leave the network and start
// seraching for a new one to join.
void emberAfPluginButtonInterfaceButton0PressedLongCallback(
  int16u button0TimePressed,
  bool pressedAtReset)
{
  emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
}

//------------------------------------------------------------------------------
// Handle the buttonPressEvent going active.  This signals that the user has
// finished a series of short button presses on the UI button, so some sort of
// network activity should take place.
void buttonPressCountEventHandler(void)
{
  if (emberAfNetworkState() != EMBER_NO_NETWORK) {
    // If on a network:
    // 2 presses activates identify
    // 3 presses blinks network status
    // 4 presses initiates a proactive rejoin
    if (consecutiveButtonPressCount == 2) {
      enableIdentify();
    } else if (consecutiveButtonPressCount == 3) {
      emberAfPluginConnectionManagerLedNetworkFoundBlink();
    } else if (consecutiveButtonPressCount == 4) {
      emberAfStartMoveCallback();
    }
  } else {
    // If not a network, then regardless of button presses or length, we want to
    // make sure we are looking for a network.
    emberAfPluginConnectionManagerResetJoinAttempts();
    if (!emberStackIsPerformingRejoin()) {
      emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
    }
  }
  
  consecutiveButtonPressCount = 0;
  
  emberEventControlSetInactive(buttonPressCountEventControl);
}

//------------------------------------------------------------------------------
// enableIdentify
// This function will cycle through all of the endpoints in the system and
// enable identify mode for each of them.
static void enableIdentify(void)
{
  int8u endpoint;
  int8u i;
  int16u identifyTimeS = 30;
  
  for (i = 0; i < emberAfEndpointCount(); i++) {
    endpoint = emberAfEndpointFromIndex(i);
    if (emberAfContainsServer(endpoint, ZCL_IDENTIFY_CLUSTER_ID)) {
      emberAfWriteAttribute(endpoint,
                            ZCL_IDENTIFY_CLUSTER_ID,
                            ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                            CLUSTER_MASK_SERVER,
                            (int8u *) &identifyTimeS,
                            ZCL_INT16U_ATTRIBUTE_TYPE);
    }
  }
}

//------------------------------------------------------------------------------
// Callback triggered when the SB1 gesture plugin receives a new gesture.  This
// will contain the gesture received and the button on which it was received.
// This function will handle all UI based state transitions, and generate radio
// radio traffic based on the user's actions.
void emberAfPluginSb1GestureSensorGestureReceivedCallback(int8u gesture,
                                                           int8u ui8SwitchNum)
{
  int8u sendNeeded;

  // Reset the frame timeout on each button press
  emberEventControlSetInactive(frameTimeoutControl);
  emberEventControlSetDelayQS(frameTimeoutControl, 4*10);

  // Clear the flag tracking whether we need to send a command
  sendNeeded = 0;

  // Form the ZigBee command to send based on the state of the device and which
  // button saw which gesture
  switch(ui8SwitchNum) {
  case SB1_GESTURE_SENSOR_SWITCH_TOP:
    switch(gesture) {
    // A touch on the top button maps to an "on" command
    case SB1_GESTURE_SENSOR_GESTURE_TOUCH:
      emberSerialPrintf(APP_SERIAL, "ZCL ON\r\n");
      emberAfFillCommandOnOffClusterOn();
      sendNeeded = 1;
      break;

    // A hold on the top button maps to an "up" command
    case SB1_GESTURE_SENSOR_GESTURE_HOLD:
      emberSerialPrintf(APP_SERIAL, "ZCL LEVEL UP\r\n");
      formCommandFromHoldUp();
      sendNeeded = 1;
      break;

    // Swiping right on the top or bottom will move the frame to the right.
    // Frame layout is: TEMP - NORMAL - HUE
    case SB1_GESTURE_SENSOR_GESTURE_SWIPE_R:
      // If the token is set to disable swiping, do nothing
      emberSerialPrintf(APP_SERIAL, "ZCL SW_R\r\n");
      if(SWIPING_ENABLED == 0) {
        emberSerialPrintf(APP_SERIAL, "Swipe disabled by token!");
      } else {
        if(tab_state == NORMAL_TAB) {
          //If the token is set to disable the color hue control, do nothing
          if(COLOR_CHANGE_ENABLED == 1) {
            tab_state = HUE_TAB;
            emberSerialPrintf(APP_SERIAL, "Switch to HUE");
            halLedBlinkSetActivityLed(BOARDLED0);
            halLedBlinkLedOn(0);
            halLedBlinkSetActivityLed(BOARDLED1);
            halLedBlinkLedOff(0);
          } else {
            emberSerialPrintf(APP_SERIAL, "Color tab disabled by token!");
          }
        } else if(tab_state == TEMP_TAB) {
          tab_state = NORMAL_TAB;
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOff(0);
          emberSerialPrintf(APP_SERIAL, "Switch to NORMAL");
        }
      }
      // Swipe commands only modify internal state, so no radio message
      // needs to be sent
      sendNeeded = 0;
      break;

    // Swiping right on the top or bottom will move the frame to the right.
    // Frame layout is: TEMP - NORMAL - HUE
    case SB1_GESTURE_SENSOR_GESTURE_SWIPE_L:
      // If the token is set to disable swiping, do nothing
      emberSerialPrintf(APP_SERIAL, "ZCL SW_L \r\n");
      if(SWIPING_ENABLED == 0) {
        emberSerialPrintf(APP_SERIAL, "Swipe disabled by token!");
      } else {
        if(tab_state == NORMAL_TAB) {
          tab_state = TEMP_TAB;
          emberSerialPrintf(APP_SERIAL, "Switch to TEMP");
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOn(0);
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
        } else if(tab_state == HUE_TAB) {
          tab_state = NORMAL_TAB;
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOff(0);
          emberSerialPrintf(APP_SERIAL, "Switch to NORMAL");
        }
      }
      sendNeeded = 0;
      break;

    // If we got here, we likely had a bad i2c transaction.  Ignore read data
    default:
      emberSerialPrintf(APP_SERIAL, "bad gesture: 0x%02x", gesture);
      sendNeeded = 0;
      return;
    }
    break;

  case SB1_GESTURE_SENSOR_SWITCH_BOTTOM:
    switch(gesture) {
    // A touch on the bottom button maps to an "off" command
    case SB1_GESTURE_SENSOR_GESTURE_TOUCH:
      emberSerialPrintf(APP_SERIAL, "ZCL OFF\r\n");
      emberAfFillCommandOnOffClusterOff();
      sendNeeded = 1;
      break;

    // A hold on the bottom button maps to a "level down" command
    case SB1_GESTURE_SENSOR_GESTURE_HOLD:
      emberSerialPrintf(APP_SERIAL, "ZCL LEVEL DOWN\r\n");
      formCommandFromHoldDown();
      sendNeeded = 1;
      break;

    // Swiping right on the top or bottom will move the frame to the right.
    // Frame layout is: TEMP - NORMAL - HUE
    case SB1_GESTURE_SENSOR_GESTURE_SWIPE_R:
      // If the token is set to disable swiping, do nothing
      emberSerialPrintf(APP_SERIAL, "ZCL SW_R\r\n");
      if(SWIPING_ENABLED == 0) {
        emberSerialPrintf(APP_SERIAL, "Swipe disabled by token!");
      } else {
        if(tab_state == NORMAL_TAB) {
          //If the token is set to disable the color hue control, do nothing
          if(COLOR_CHANGE_ENABLED == 1) {
            tab_state = HUE_TAB;
            emberSerialPrintf(APP_SERIAL, "Switch to HUE");
            halLedBlinkSetActivityLed(BOARDLED0);
            halLedBlinkLedOn(0);
            halLedBlinkSetActivityLed(BOARDLED1);
            halLedBlinkLedOff(0);
          } else {
            emberSerialPrintf(APP_SERIAL, "Color tab disabled by token!");
          }
        } else if(tab_state == TEMP_TAB) {
          tab_state = NORMAL_TAB;
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOff(0);
          emberSerialPrintf(APP_SERIAL, "Switch to NORMAL");
        }
      }
      // Swipe commands only modify internal state, so no radio message
      // needs to be sent
      sendNeeded = 0;
      break;

    // Swiping right on the top or bottom will move the frame to the right.
    // Frame layout is: TEMP - NORMAL - HUE
    case SB1_GESTURE_SENSOR_GESTURE_SWIPE_L:
      // If the token is set to disable swiping, do nothing
      emberSerialPrintf(APP_SERIAL, "ZCL SW_L \r\n");
      if(SWIPING_ENABLED == 0) {
        emberSerialPrintf(APP_SERIAL, "Swipe disabled by token!");
      } else {
        if(tab_state == NORMAL_TAB) {
          tab_state = TEMP_TAB;
          emberSerialPrintf(APP_SERIAL, "Switch to TEMP");
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOn(0);
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
        } else if(tab_state == HUE_TAB) {
          tab_state = NORMAL_TAB;
          halLedBlinkSetActivityLed(BOARDLED0);
          halLedBlinkLedOff(0);
          halLedBlinkSetActivityLed(BOARDLED1);
          halLedBlinkLedOff(0);
          emberSerialPrintf(APP_SERIAL, "Switch to NORMAL");
        }
      }
      sendNeeded = 0;
      break;

      // If we got here, we likely had a bad i2c transaction.  Ignore read data
      default:
        emberSerialPrintf(APP_SERIAL, "bad gesture: 0x%02x", gesture);
        sendNeeded = 0;
        return;
    }
    break;

  // If we got here, we likely had a bad i2c transaction.  Ignore read data
  default:
    emberSerialPrintf(APP_SERIAL, "unknown button: 0x%02x\r\n", ui8SwitchNum);
    sendNeeded = 0;
    return;
  }

  // transmit the formed command if we deteremined a command send was needed
  if(sendNeeded) {
    //blink the LED to show that a gesture was recognized
    halLedBlinkBlink( 1, 100);
    localSendCommand();
  }
}

//------------------------------------------------------------------------------
// FrameTimeout event handler
// This handler is called a long time (default 10 seconds) after the user
// swipes left or right.  It will return the switch to the normal on/off tab.
void frameTimeoutHandler( void )
{
  //Make sure we don't stay on the HUE/TEMP tab for too long
  if((tab_state == HUE_TAB) || (tab_state == TEMP_TAB)) {
    emberSerialPrintf(APP_SERIAL, "Tab timeout, back to normal\n");
    tab_state = NORMAL_TAB;
    halLedBlinkSetActivityLed(BOARDLED0);
    halLedBlinkLedOff(0);
    halLedBlinkSetActivityLed(BOARDLED1);
    halLedBlinkLedOff(0);
  }
  emberEventControlSetInactive(frameTimeoutControl);
}

//------------------------------------------------------------------------------
// Main Tick
// Whenever main application tick is called, this callback will be called at the
// end of the main tick execution.  It ensure that no LED activity will occur
// when the LED_ENABLED macro is set to disabled mode.
void emberAfMainTickCallback(void)
{
  //Use blinking led to indicate which tab is active, unless disabled by tokens
  if(LED_ENABLED == 0) {
    halLedBlinkSetActivityLed(BOARDLED0);
    halLedBlinkLedOff(0);
    halLedBlinkSetActivityLed(BOARDLED1);
    halLedBlinkLedOff(0);
  }
}

//------------------------------------------------------------------------------
// Send a Zigbee level control, move to color, or move to color temperature
// command, depending on the active tab.
void formCommandFromHoldUp(void)
{
  switch(tab_state){
  case NORMAL_TAB:
    emberSerialPrintf(APP_SERIAL, "DIM UP \r\n");
    emberAfFillCommandLevelControlClusterStepWithOnOff(0,
                                                       dimStepSize,
                                                       dimTime);
    break;
  case TEMP_TAB:
    emberSerialPrintf(APP_SERIAL, "TEMP UP \r\n");
    
    emberAfFillCommandColorControlClusterStepColorTemperatue(
      COLOR_TEMP_STEP_POSITIVE,
      COLOR_TEMP_STEP_AMOUNT,
      0,  // transitionTime = 0
      0,  // colorTemperatureMinimum = 0, as it will be handled by the gateway
      0xFFFF); // colorTemperatureMaximum This will be handled by the gateway

    break;
  case HUE_TAB:
    emberSerialPrintf(APP_SERIAL, "HUE UP \r\n");
    
    emberAfFillCommandColorControlClusterStepHue(COLOR_HUE_STEP_POSITIVE,
                                                 COLOR_HUE_STEP_AMOUNT,
                                                 0); //Transition time = 0
    break;
  }
}

//------------------------------------------------------------------------------
// Send a Zigbee level control, move to color, or move to color temperature
// command, depending on the active tab.
void formCommandFromHoldDown(void)
{
  switch(tab_state){
  case NORMAL_TAB:
    emberSerialPrintf(APP_SERIAL, "DIM DOWN \r\n");
    emberAfFillCommandLevelControlClusterStepWithOnOff(1,
                                                       dimStepSize,
                                                       dimTime);

    break;
  case TEMP_TAB:
    emberSerialPrintf(APP_SERIAL, "TEMP DN \r\n");
    emberAfFillCommandColorControlClusterStepColorTemperatue(
      COLOR_TEMP_STEP_NEGATIVE,
      COLOR_TEMP_STEP_AMOUNT,
      0,  // transitionTime = 0
      0,  // colorTemperatureMinimum = 0, as it will be handled by the gateway
      0xFFFF); // colorTemperatureMaximum = 0, as it will be handled by the gateway
    break;
  case HUE_TAB:
    emberSerialPrintf(APP_SERIAL, "HUE DN \r\n");
    emberAfFillCommandColorControlClusterStepHue(COLOR_HUE_STEP_NEGATIVE,
                                                 COLOR_HUE_STEP_AMOUNT,
                                                 0); //Transition time = 0
    break;
  }
}

//------------------------------------------------------------------------------
// localSendCommand
// Build a zigbee command and transmit it to the network.
void localSendCommand(void)
{
  int8u srcEndpoint;
  EmberStatus status;

  // Sanity check the environment

  // make sure that we have a network on which to broadcast the command
  if (emberNetworkState() != EMBER_JOINED_NETWORK) {
    emberSerialPrintf(APP_SERIAL, "Can not send command: not on a newtork!");
    return;
  }

  srcEndpoint = emberAfEndpointFromIndex(LIGHTING_CLIENT_ENDPOINT_INDEX);
  //globalApsFrame.options |= EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY;
  
  // Remote endpoint need not be set, since it will be provided by the call to
  // emberAfSendCommandUnicastToBindings
  emberAfSetCommandEndpoints(srcEndpoint, 0);
  
  // Transmit the command to the network
  status = emberAfSendCommandUnicastToBindings();

  // Verify the command was sent successfully
  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Error sending msg to binding entries: 0x%X",
                       status);
  }
  emberAfDebugPrint("TX buffer: [");
  emberAfDebugFlush();
  emberAfDebugPrintBuffer(appZclBuffer, appZclBufferLen, TRUE);
  emberAfDebugPrintln("]");
  emberAfDebugFlush();

  // Reset global state variables in preparation for building the next command
  zclCmdIsBuilt = FALSE;
  mfgSpecificId = EMBER_AF_NULL_MANUFACTURER_CODE;
}

//------------------------------------------------------------------------------
// Ok To Sleep
//
// This function is called by the Idle/Sleep plugin before sleeping.  It is
// called with interrupts disabled.  The application should return TRUE if the
// device may sleep or FALSE otherwise.
//
// param durationMs The maximum duration in milliseconds that the device will
// sleep.  Ver.: always
boolean emberAfPluginIdleSleepOkToSleepCallback(int32u durationMs)
{
  if( halSb1GestureSensorCheckForMsg() ) {
    return FALSE;
  } else {
    return TRUE;
  }
}

//------------------------------------------------------------------------------
// Ok To Idle
//
// This function is called by the Idle/Sleep plugin before idling.  It is called
// with interrupts disabled.  The application should return TRUE if the device
// may idle or FALSE otherwise.
boolean emberAfPluginIdleSleepOkToIdleCallback(void)
{
  if( halSb1GestureSensorCheckForMsg() ) {
    return FALSE;
  } else {
    return TRUE;
  }
}

