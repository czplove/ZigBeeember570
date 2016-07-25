/***************************************************************************//**
 * @file aesdrv_common_crypto.h
 * @brief AESDRV internal API for function used by all CRYPTO algoirhtms
 * @version x.x.x
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2014 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#ifndef __SILICON_LABS_AESDRV_COMMON_CRYPTO_H
#define __SILICON_LABS_AESDRV_COMMON_CRYPTO_H

#include "em_device.h"
#include "aesdrv.h"

Ecode_t AESDRV_HWAccelSet(AESDRV_Config_t * config);
AESDRV_HWAcceleration_t AESDRV_HWAccelGet(void);
void AESDRV_HwAccelSetup(uint8_t * pData,
                         uint32_t authDataLength,
                         uint32_t textLength);

#endif /* __SILICON_LABS_AESDRV_COMMON_CRYPTO_H */
