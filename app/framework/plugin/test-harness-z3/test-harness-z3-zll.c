//
// test-harness-z3-zll.c
//
// August 3, 2015
// Refactored November 23, 2015
//
// ZigBee 3.0 touchlink test harness functionality
//

// To pull the ZLL types in, af-types.h required EMBER_AF_PLUGIN_ZLL_COMMISSIONING
// to be #define'd. In unit tests, we don't get this. So fake it.
#ifdef EMBER_SCRIPTED_TEST
  #define EMBER_AF_PLUGIN_ZLL_COMMISSIONING
#endif

#include "app/framework/include/af.h"

#include "test-harness-z3-core.h"
#include "test-harness-z3-zll.h"

#include "app/framework/plugin/zll-commissioning/zll-commissioning.h"

// -----------------------------------------------------------------------------
// Globals

// We need this extern'd for unit tests since the zll-commissioning.h extern
// might not be compiled in.
#if defined(EMBER_SCRIPTED_TEST)
extern uint32_t emAfZllSecondaryChannelMask;
#endif

static EmberZllNetwork zllNetwork = { {0,}, {0,0,0}, };
#define initZllNetwork(network)                                             \
  do { MEMMOVE(&zllNetwork, network, sizeof(EmberZllNetwork)); } while (0);
#define deinitZllNetwork()                                                  \
  do { zllNetwork.securityAlgorithm.transactionId = 0; } while (0);
#define zllNetworkIsInit()                                                  \
  (zllNetwork.securityAlgorithm.transactionId != 0)

#define STATE_PRIMARY_CHANNELS (0x01)
#define STATE_SCANNING         (0x02)
static uint8_t state;

// -----------------------------------------------------------------------------
// Util

static EmberStatus startScan(void)
{
  EmberNodeType nodeType;

  nodeType = (((emAfPluginTestHarnessZ3DeviceMode
                == EM_AF_PLUGIN_TEST_HARNESS_Z3_DEVICE_MODE_ZR_NOT_ADDRESS_ASSIGNABLE)
               || (emAfPluginTestHarnessZ3DeviceMode
                   == EM_AF_PLUGIN_TEST_HARNESS_Z3_DEVICE_MODE_ZR_ADDRESS_ASSIGNABLE))
              ? EMBER_ROUTER
              : EMBER_END_DEVICE);

  if ((emAfPluginTestHarnessZ3DeviceMode
       == EM_AF_PLUGIN_TEST_HARNESS_Z3_DEVICE_MODE_ZR_NOT_ADDRESS_ASSIGNABLE)
      || (emAfPluginTestHarnessZ3DeviceMode
          == EM_AF_PLUGIN_TEST_HARNESS_Z3_DEVICE_MODE_ZED_NOT_ADDRESS_ASSIGNABLE)) {
    EmberTokTypeStackZllData token;
    emberZllGetTokenStackZllData(&token);
    token.bitmask &= ~EMBER_ZLL_STATE_ADDRESS_ASSIGNMENT_CAPABLE;
    emberZllSetTokenStackZllData(&token);
  }

  return emberZllStartScan(emAfZllPrimaryChannelMask | emAfZllSecondaryChannelMask,
                           0, // default power
                           nodeType);
}

// -----------------------------------------------------------------------------
// Touchlink CLI Commands

// plugin test-harness z3 touchlink scan-request-process <linkInitiator:1>
// <unused:1> <options:4>
void emAfPluginTestHarnessZ3TouchlinkScanRequestProcessCommand(void)
{
  EmberStatus status = EMBER_INVALID_CALL;

#ifndef EZSP_HOST
  uint8_t linkInitiator = (uint8_t)emberUnsignedCommandArgument(0);
  uint32_t options      = emAfPluginTestHarnessZ3GetSignificantBit(2);

  if (options & BIT(0)) {
    emAfZllPrimaryChannelMask = BIT32(11);
#if defined(EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_SECONDARY_CHANNELS) \
    || defined(EMBER_SCRIPTED_TEST)
    emAfZllSecondaryChannelMask = 0;
#endif
  }

  // We currently don't have a way to use the link initiator option.
  (void)linkInitiator;

  if (options & BIT(3)) {
    // ignore scan-requests
    emberZllSetPolicy(EMBER_ZLL_POLICY_DISABLED);
    status = EMBER_SUCCESS;
  } else {
    emberZllSetPolicy(EMBER_ZLL_POLICY_ENABLED);
    emberAfZllSetInitialSecurityState();
    state = (STATE_PRIMARY_CHANNELS | STATE_SCANNING);
    status = startScan();
  }
#endif /* EZSP_HOST */

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Scan request process",
                     status);
}

void emAfPluginTestHarnessZ3TouchlinkStartAsRouterCommand(void)
{
  EmberPanId panId = (EmberPanId)emberUnsignedCommandArgument(0);
  uint32_t options = emAfPluginTestHarnessZ3GetSignificantBit(1);
  EmberStatus status = EMBER_INVALID_CALL;

  // This options bitmask is currently unused.
  (void)options;

  if (zllNetworkIsInit()) {
    status = emAfZllFormNetwork(zllNetwork.zigbeeNetwork.channel, 0, panId);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                    TEST_HARNESS_Z3_PRINT_NAME,
                    "Start as router",
                    status);
}

// plugin test-harness z3 touchlink is-scanning
void emAfPluginTestHarnessZ3TouchlinkIsScanningCommand(void)
{
  emberAfCorePrintln("scanning:%p",
                     (state & STATE_SCANNING ? "true" : "false"));
}

// plugin test-harness z3 touchlink device-information-request
// <startIndex:1> <options:4>
void emAfPluginTestHarnessZ3TouchlinkDeviceInformationRequestCommand(void)
{
  uint8_t startIndex = (uint8_t)emberUnsignedCommandArgument(0);
  uint32_t options   = emAfPluginTestHarnessZ3GetSignificantBit(1);
  EmberStatus status = EMBER_INVALID_CALL;

  uint32_t interpanTransactionId = zllNetwork.securityAlgorithm.transactionId;
  if (options & BIT(0)) {
    interpanTransactionId --;
  }

  if (zllNetworkIsInit()) {
    emberAfFillExternalBuffer(EM_AF_PLUGIN_TEST_HARNESS_Z3_ZLL_CLIENT_TO_SERVER_FRAME_CONTROL,
                              ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                              ZCL_DEVICE_INFORMATION_REQUEST_COMMAND_ID,
                              "wu",
                              interpanTransactionId,
                              startIndex);
    status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                        zllNetwork.eui64,
                                        EMBER_NULL_NODE_ID,    // node id - ignored
                                        0x0000,                // group id - ignored
                                        EMBER_ZLL_PROFILE_ID);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Device information request",
                     status);
}

// plugin test-harness z3 touchlink identify-request <duration:2> <options:4>
void emAfPluginTestHarnessZ3TouchlinkIdentifyRequestCommand(void)
{
  uint16_t duration = (uint16_t)emberUnsignedCommandArgument(0);
  uint32_t options  = emAfPluginTestHarnessZ3GetSignificantBit(1);
  EmberStatus status = EMBER_INVALID_CALL;

  uint32_t interpanTransactionId = zllNetwork.securityAlgorithm.transactionId;
  if (options & BIT(0)) {
    interpanTransactionId --;
  }

  if (zllNetworkIsInit()) {
    emberAfFillExternalBuffer(EM_AF_PLUGIN_TEST_HARNESS_Z3_ZLL_CLIENT_TO_SERVER_FRAME_CONTROL,
                              ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                              ZCL_IDENTIFY_REQUEST_COMMAND_ID,
                              "wv",
                              interpanTransactionId,
                              duration);
    status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                        zllNetwork.eui64,
                                        EMBER_NULL_NODE_ID,    // node id - ignored
                                        0x0000,                // group id - ignored
                                        EMBER_ZLL_PROFILE_ID);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Identify request",
                     status);
}

// plugin test-harness z3 touchlink reset-to-factory-new-request <options:4>
void emAfPluginTestHarnessZ3TouchlinkRTFNRequestCommand(void)
{
  uint32_t options  = emAfPluginTestHarnessZ3GetSignificantBit(0);
  EmberStatus status = EMBER_INVALID_CALL;

  // We currently do not have a way to use the option bitmask.
  (void)options;

  if (zllNetworkIsInit()) {
    emberAfFillExternalBuffer(EM_AF_PLUGIN_TEST_HARNESS_Z3_ZLL_CLIENT_TO_SERVER_FRAME_CONTROL,
                              ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                              ZCL_RESET_TO_FACTORY_NEW_REQUEST_COMMAND_ID,
                              "w",
                              zllNetwork.securityAlgorithm.transactionId);
    status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                        zllNetwork.eui64,
                                        EMBER_NULL_NODE_ID,    // node id - ignored
                                        0x0000,                // group id - ignored
                                        EMBER_ZLL_PROFILE_ID);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Reset to factory new request",
                     status);
}

// plugin test-harness z3 touchlink network-start-request <dstAddress:2>
// <freeAddrBegin:2> <freeAddrEnd:2> <groupIdBegin:2> <groupIdEnd:2> <options:4>
// plugin test-harness z3 touchlink network-join-router-request <dstAddress:2>
// <freeAddrBegin:2> <freeAddrEnd:2> <groupIdBegin:2> <groupIdEnd:2> <options:4>
// plugin test-harness z3 touchlink network-join-end-device-request <dstAddress:2>
// <freeAddrBegin:2> <freeAddrEnd:2> <groupIdBegin:2> <groupIdEnd:2> <options:4>
void emAfPluginTestHarnessZ3TouchlinkNetworkCommand(void)
{
  EmberNodeId nodeId              = (EmberNodeId)emberUnsignedCommandArgument(0);
  EmberNodeId freeAddrBegin       = (EmberNodeId)emberUnsignedCommandArgument(1);
  EmberNodeId freeAddrEnd         = (EmberNodeId)emberUnsignedCommandArgument(2);
  EmberMulticastId freeGroupBegin = (EmberMulticastId)emberUnsignedCommandArgument(3);
  EmberMulticastId freeGroupEnd   = (EmberMulticastId)emberUnsignedCommandArgument(4);
  uint32_t options                = emAfPluginTestHarnessZ3GetSignificantBit(5);

  uint8_t command = 0;
  EmberTokTypeStackZllData token;
  EmberStatus status = EMBER_INVALID_CALL;
  EmberZllNetwork mangledNetwork;

  // We currently do not have a way to use the nodeId.
  (void)nodeId;

  if (!zllNetworkIsInit()) {
    goto done;
  }

  switch (emberStringCommandArgument(-1, NULL)[13]) {
  case '-':
    command = ZCL_NETWORK_START_REQUEST_COMMAND_ID;
    break;
  case 'r':
    command = ZCL_NETWORK_JOIN_ROUTER_REQUEST_COMMAND_ID;
    break;
  case 'e':
    command = ZCL_NETWORK_JOIN_END_DEVICE_REQUEST_COMMAND_ID;
    break;
  default:
    status = EMBER_BAD_ARGUMENT;
    goto done;
  }

  MEMMOVE(&mangledNetwork, &zllNetwork, sizeof(EmberZllNetwork));

  emberZllGetTokenStackZllData(&token);
  token.freeNodeIdMin  = freeAddrBegin;
  token.freeNodeIdMax  = freeAddrEnd;
  token.freeGroupIdMin = freeGroupBegin;  
  token.freeGroupIdMax = freeGroupEnd;
  if (command == ZCL_NETWORK_START_REQUEST_COMMAND_ID) {
    // Say that we are FN so that we will indeed send a network start.
    token.bitmask |= EMBER_ZLL_STATE_FACTORY_NEW;
  } else {
    // Say that we are not FN so we will send a network join.
    token.bitmask &= ~EMBER_ZLL_STATE_FACTORY_NEW;
    // Mangle the target device type so that we get the command we want.
    mangledNetwork.nodeType
      = (command == ZCL_NETWORK_JOIN_ROUTER_REQUEST_COMMAND_ID
         ? EMBER_ROUTER
         : EMBER_END_DEVICE);
  }
  emberZllSetTokenStackZllData(&token);

  if (options & BIT(0)) {
    mangledNetwork.securityAlgorithm.transactionId --;
  }

  emberAfZllSetInitialSecurityState();
  status = emberZllJoinTarget(&mangledNetwork);

 done:
  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     (command == ZCL_NETWORK_START_REQUEST_COMMAND_ID
                      ? "Network start request"
                      : (command == ZCL_NETWORK_JOIN_ROUTER_REQUEST_COMMAND_ID
                         ? "Network join router request"
                         : "Network join end device request")),
                     status);
}

// plugin test-harness z3 touchlink network-update-request <options:4>
void emAfPluginTestHarnessZ3TouchlinkNetworkUpdateRequestCommand(void)
{
  uint32_t options = emAfPluginTestHarnessZ3GetSignificantBit(0);

  EmberStatus status = EMBER_INVALID_CALL;
  EmberNodeType nodeType;
  EmberNetworkParameters networkParameters;
  uint8_t mangledNwkUpdateId;

  emberAfGetNetworkParameters(&nodeType, &networkParameters);

  mangledNwkUpdateId = networkParameters.nwkUpdateId;
  if (options == BIT32(0)) {
    mangledNwkUpdateId += 5;
  }

  if (zllNetworkIsInit()) {
    emberAfFillExternalBuffer(EM_AF_PLUGIN_TEST_HARNESS_Z3_ZLL_CLIENT_TO_SERVER_FRAME_CONTROL,
                              ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                              ZCL_NETWORK_UPDATE_REQUEST_COMMAND_ID,
                              "w8uuvv",
                              zllNetwork.securityAlgorithm.transactionId,
                              networkParameters.extendedPanId,
                              mangledNwkUpdateId,
                              networkParameters.radioChannel,
                              networkParameters.panId,
                              emberAfGetNodeId());
    status = emberAfSendCommandInterPan(0xFFFF, // destination pan id
                                        zllNetwork.eui64,
                                        EMBER_NULL_NODE_ID, // node id - ignored
                                        0x0000,             // group id - ignored
                                        EMBER_ZLL_PROFILE_ID);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Network update request",
                     status);
}

// -----------------------------------------------------------------------------
// Framework callbacks

void emAfPluginTestHarnessZ3ZllNetworkFoundCallback(const EmberZllNetwork *networkInfo)
{
  initZllNetwork(networkInfo);
}

void emAfPluginTestHarnessZ3ZllScanCompleteCallback(EmberStatus status)
{
  if (!zllNetworkIsInit() && (state & STATE_PRIMARY_CHANNELS)) {
    state &= ~STATE_PRIMARY_CHANNELS;
    startScan();
  } else {
    state &= ~STATE_SCANNING;
    if (status != EMBER_SUCCESS) {
      deinitZllNetwork();
    } else {
      extern EmberStatus emSetLogicalAndRadioChannel(uint8_t channel);
      emSetLogicalAndRadioChannel(zllNetwork.zigbeeNetwork.channel);
    }
  }
}
