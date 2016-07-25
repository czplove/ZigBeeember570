/*
 *
 * File: gp-proxy-table.c
 * Description: GP proxy table related code.
 *
 * Author(s): Jeffrey Rosenberger, jeffrey.rosenberger@silabs.com
 *
 * Copyright 2015 Silicon Laboratories, Inc.                                *80*
 */
#if 0
#define EMBER_AF_PLUGIN_GP_LIBRARY
#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/core/ember-stack.h"
#include "stack/include/gp-types.h"
//#include "gp.h"
//#include "gp-command.h"
#include "framework/packet-header.h"

#include "core/scripted-stub.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "stack/mac/802.15.4/crypto.h"
#include "stack/platform/micro/aes.h"
#include "green-power-proxy-table.h"
EmberGpProxyTableEntry proxyTable[EMBER_GP_PROXY_TABLE_SIZE];
EmberGpProxyTableEntry *emGpProxyTable = proxyTable;
uint8_t emGpProxyTableSize = EMBER_GP_PROXY_TABLE_SIZE;

#endif
