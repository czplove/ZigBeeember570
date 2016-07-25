/*
 * File: bootloader-interface.c
 * Description: EM3XX common bootloader HAL functions
 *
 * Copyright 2008-2009 by Ember Corporation. All rights reserved.           *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/micro/bootloader-interface.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/common/bootloader-common.h"

//////////////////////
// Generic bootloader functionality
///////////

/** @description Returns the bootloader type the application was
 *  built for.
 *
 * @return BL_TYPE_NULL, BL_TYPE_STANDALONE or BL_TYPE_APPLICATION
 */
BlBaseType halBootloaderGetType(void)
{
  return 0;
}


// Returns the extended type of 
BlExtendedType halBootloaderGetInstalledType(void)
{
  return 0;
}

uint16_t halGetBootloaderVersion(void)
{
  return 0;
}

void halGetExtendedBootloaderVersion(uint32_t* getEmberVersion, uint32_t* customerVersion)
{
}
