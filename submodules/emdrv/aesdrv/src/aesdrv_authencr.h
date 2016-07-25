/***************************************************************************//**
 * @file aesdrv_authencr.h
 * @brief Definitions for authentication and encryption algorithms common to
 *        all crypto devices (AES, CRYPTO, etc.)
 *
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

#ifndef __SILICON_LABS_AESDRV_AUTHENCR_H
#define __SILICON_LABS_AESDRV_AUTHENCR_H

Ecode_t AESDRV_CCM_Generalized(AESDRV_CCM_Context_t*   pCcmContext,
                               const uint8_t*          pDataInput,
                                     uint8_t*          pOutputData,
                               const uint32_t          dataLength,
                               const uint8_t*          pHdr,
                               const uint32_t          hdrLength,
                               const uint8_t*          pKey,
                               const uint32_t          keyLength,
                               const uint8_t*          pNonce,
                               const uint32_t          nonceLength,
                               uint8_t*                pAuthTag,
                               const uint8_t           authTagLength,
                               const bool              encrypt,
                               const bool              encryptedPayload,
                               AESDRV_AsynchCallback_t asynchCallback,
                               void*                   asynchCallbackArgument);

#endif /* __SILICON_LABS_AESDRV_AUTHENCR_H */
