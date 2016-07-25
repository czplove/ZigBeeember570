//
// test-harness-z3-aps.c
//
// August 3, 2015
// Refactored November 23, 2015
//
// ZigBee 3.0 aps test harness functionality
//

#include "app/framework/include/af.h"

#include "stack/include/packet-buffer.h"

#include "test-harness-z3-core.h"

// -----------------------------------------------------------------------------
// Constants

#define APS_REQUEST_KEY_COMMAND (0x08)

#define ZTT_INSTALL_CODE_KEY {{0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,   \
                               0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,}}

// -----------------------------------------------------------------------------
// APS CLI Commands

#ifdef EZSP_HOST
  //#define emberSendRemoveDevice(...) EMBER_INVALID_CALL
  #define emSendApsCommand(...)      EMBER_INVALID_CALL
#else

// Internal stack API.
extern bool emSendApsCommand(EmberNodeId destination,
                             EmberEUI64 longDestination,
                             EmberMessageBuffer payload,
                             uint8_t options);

#endif /* EZSP_HOST */

// plugin test-harness z3 aps aps-remove-device <parentLong:8> <dstLong:8>
void emAfPluginTestHarnessZ3ApsApsRemoveDevice(void)
{
  EmberEUI64 parentLong, targetLong;
  EmberNodeId parentShort;
  EmberStatus status;

  emberCopyBigEndianEui64Argument(0, parentLong);
  emberCopyBigEndianEui64Argument(1, targetLong);

  parentShort = emberLookupNodeIdByEui64(parentLong);
  if (parentShort == EMBER_NULL_NODE_ID) {
    status = EMBER_BAD_ARGUMENT;
  } else {
    status = emberSendRemoveDevice(parentShort, parentLong, targetLong);
  }

  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Remove device",
                     status);
}

// plugin test-harness z3 aps aps-request-key <dstShort:2> <keyType:1>
// <parentLong:8> <options:4>
void emAfPluginTestHarnessZ3ApsApsRequestKeyCommand(void)
{
  EmberStatus status = EMBER_INVALID_CALL;
#ifndef EZSP_HOST
  EmberNodeId destShort = (EmberNodeId)emberUnsignedCommandArgument(0);
  uint8_t keyType       = (uint8_t)emberUnsignedCommandArgument(1);
  uint32_t options      = emAfPluginTestHarnessZ3GetSignificantBit(3);
  EmberEUI64 partnerLong, trustCenterEui64;
  uint8_t frame[10];
  uint8_t *finger = &frame[0];
  EmberMessageBuffer commandBuffer;
  uint8_t apsCommandOptions;

  emberCopyBigEndianEui64Argument(2, partnerLong);

  status = emberLookupEui64ByNodeId(EMBER_TRUST_CENTER_NODE_ID, trustCenterEui64);
  if (status != EMBER_SUCCESS) {
    goto done;
  }

  // What key should we be using?
  if (options & BIT32(5)) {
    EmberKeyData zttInstallCodeKey = ZTT_INSTALL_CODE_KEY;
    status = emberAddOrUpdateKeyTableEntry(trustCenterEui64,
                                           false,
                                           &zttInstallCodeKey);
  }
  if (status != EMBER_SUCCESS) {
    goto done;
  }

  // What is the key type?
  if (options & BIT32(7)) {
    keyType = 0x01; // network key
  } else if (options & BIT32(8)) {
    keyType = 0x02; // app link key
  }

  *finger++ = APS_REQUEST_KEY_COMMAND;
  *finger++ = keyType;
  if (keyType == 0x02 && !(options & BIT32(0))) { // app link key type
    MEMMOVE(finger, partnerLong, EUI64_SIZE);
    finger += EUI64_SIZE;
  }

  commandBuffer = emberFillLinkedBuffers(frame, finger - &frame[0]);
  if (commandBuffer == EMBER_NULL_MESSAGE_BUFFER) {
    status = EMBER_NO_BUFFERS;
    goto done;
  }

  // nwk encryption and aps encryption or not if option bit 6 is set
  apsCommandOptions = ((options & BIT32(6)) ? 0x00 : 0x01 | 0x02);
  status = (emSendApsCommand(destShort,
                             trustCenterEui64,
                             commandBuffer,
                             apsCommandOptions)
            ? EMBER_SUCCESS
            : EMBER_ERR_FATAL);
  emberReleaseMessageBuffer(commandBuffer);

#endif /* EZSP_HOST */

 done:
  emberAfCorePrintln("%p: %p: 0x%X",
                     TEST_HARNESS_Z3_PRINT_NAME,
                     "Request key",
                     status);
}
