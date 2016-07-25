/***************************************************************************//**
 * @file aesdrv.h
 * @brief AESDRV API definition.
 * @version x.x.x
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#ifndef __SILICON_LABS_AESDRV_H
#define __SILICON_LABS_AESDRV_H

#include "em_device.h"
#if ( defined(CRYPTO_COUNT) && (CRYPTO_COUNT > 0) ) || \
  ( defined(AES_COUNT) && (AES_COUNT > 0) )

#include "stdint.h"
#include "stdbool.h"
#include "ecode.h"
#include "cryptolib.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup CRYPTOLIB
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup AESDRV
 * @brief AESDRV AES driver
 * @{
 ******************************************************************************/
  
/*******************************************************************************
 ******************************    ERROR CODES    ******************************
 ******************************************************************************/
#define ECODE_EMDRV_AESDRV_NOT_SUPPORTED         (ECODE_EMDRV_AESDRV_BASE | 0x1)
#define ECODE_EMDRV_AESDRV_AUTHENTICATION_FAILED (ECODE_EMDRV_AESDRV_BASE | 0x2)
#define ECODE_EMDRV_AESDRV_OUT_OF_RESOURCES      (ECODE_EMDRV_AESDRV_BASE | 0x3)
#define ECODE_EMDRV_AESDRV_INVALID_PARAM         (ECODE_EMDRV_AESDRV_BASE | 0x4)

/*******************************************************************************
 *******************************   TYPEDEFS   **********************************
 ******************************************************************************/

/** Enum defines which hw acceleration to use for moving data to/from CRYPTO. */
typedef enum
{
  aesdrvHWAccelerationNone, /** Core CPU moves data to/from the CRYPTO data
                                registers. */
  aesdrvHWAccelerationBUFC,  /** Buffer Controller moves data to/from the CRYPTO
                                data registers. */
  aesdrvHWAccelerationDMA   /** DMA moves data to/from the CRYPTO data
                                registers. */
} AESDRV_HWAcceleration_t;

/** BUFC acceleration specific config structure. */
typedef struct
{
  uint8_t bufId;
} AESDRV_BufcConfig_t;

/** HW acceleration config structure. */
typedef union
{
  AESDRV_BufcConfig_t bufc;
} AESDRV_ModeSpecificConfig_t;

/** AES config structure used to initialize AES driver. */
typedef struct
{
  AESDRV_HWAcceleration_t     hwAcceleration;
  AESDRV_ModeSpecificConfig_t specificConfig;
  bool                        authTagOptimize; /**< Enable/disable optimized 
                                                  handling of authentication tag
                                                  in CCM/GCM. Tag optimization
                                                  expects tag size 0,4,8,12 or
                                                  16 bytes.*/
} AESDRV_Config_t;

/** Prototype of counter callback function provided by user. */
typedef void (*AESDRV_CtrCallback_t)(uint8_t *ctr);

/***************************************************************************//**
 * @brief
 *  AESDRV asynchronous (non-blocking) operation completion callback function.
 *
 * @details
 *  The callback function is called when an asynchronous (non-blocking)
 *  AES operation has completed.
 *
 * @param[in] result
 *  The result of the AES operation.
 *
 * @param[in] userArgument
 *  Optional user defined argument supplied when starting the asynchronous
 *  AES operation.
 ******************************************************************************/
typedef void (*AESDRV_AsynchCallback_t)(Ecode_t result, void* userArgument);

/** AESDRV Context structures */
typedef struct
{
  unsigned int               remainingBlocks;
  uint32_t*                  pBlockIn;
  uint32_t*                  pBlockOut;
  uint8_t*                   ctrPointer;
  AESDRV_AsynchCallback_t    asynchCallback;
  void*                      asynchCallbackArgument;
} AESDRV_BlockCipherAsynchContext_t;

typedef enum
{
  asynchModeCcm,
  asynchModeCcmBle,
  asynchModeGcm
} AESDRV_CM_AsynchMode_t;

typedef struct
{
  const uint8_t*             pHdr;
  const uint8_t*             pDataInput;
  uint8_t*                   pDataOutput;
  uint32_t                   la;
  uint32_t                   lm;
  uint32_t                   hdrLength;
  uint32_t                   dataLength;
  uint8_t*                   pAuthTag;
  uint8_t                    authTagLength;
  bool                       encryptingHeader;
  bool                       encrypt;
  AESDRV_CM_AsynchMode_t     asynchMode;
  AESDRV_AsynchCallback_t    asynchCallback;
  void*                      asynchCallbackArgument;
} AESDRV_CCM_AsynchContext_t, AESDRV_GCM_AsynchContext_t;
  
typedef struct
{
  uint32_t*                  dataPointer;
  uint32_t                   dataBlocks;
  uint32_t                   lastBlock[4];
  uint8_t*                   digest;
  uint16_t                   digestLengthBits;
  bool                       encrypt;
  AESDRV_AsynchCallback_t    asynchCallback;
  void*                      asynchCallbackArgument;
} AESDRV_CMAC_AsynchContext_t;


#if ( defined(CRYPTO_COUNT) && (CRYPTO_COUNT > 0) )
#include "cryptodrv.h"

typedef struct
{
  CRYPTODRV_Context_t        cryptodrvContext; /**< Context of CRYPTO driver */
} AESDRV_CommonContext_t;
  
typedef struct
{
  AESDRV_CommonContext_t            commonContext; /**< AESDRV common context */
  AESDRV_BlockCipherAsynchContext_t asynchContext; /**< Context of asynchronous
                                                      AES blockcipher operations
                                                   */
} AESDRV_BlockCipherContext_t;

typedef struct
{
  AESDRV_CommonContext_t      commonContext;    /**< AESDRV common context */
  AESDRV_CCM_AsynchContext_t  asynchContext;    /**< Context of asynchronous
                                                   CCM operations. */
} AESDRV_CCM_Context_t;

typedef struct
{
  AESDRV_CommonContext_t      commonContext;    /**< AESDRV common context */
  AESDRV_GCM_AsynchContext_t  asynchContext;    /**< Context of asynchronous
                                                   GCM operations. */
} AESDRV_GCM_Context_t;

typedef struct
{
  AESDRV_CommonContext_t      commonContext;    /**< AESDRV common context */
  AESDRV_CMAC_AsynchContext_t asynchContext;    /**< Context of asynchronous
                                                   CMAC operations. */
} AESDRV_CMAC_Context_t;

#elif ( defined(AES_COUNT) && (AES_COUNT > 0) )

typedef void* AESDRV_CommonContext_t;
typedef void* AESDRV_BlockCipherContext_t;
typedef void* AESDRV_CCM_Context_t;
typedef void* AESDRV_GCM_Context_t;
typedef void* AESDRV_CMAC_Context_t;
  
#endif
  
/*******************************************************************************
 ******************************   Functions   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *   Initialize the AESDRV module.
 *
 * @details
 *   Initializes AESDRV based on the given configuration structure.
 *   In case of AES HW module there is nothing to be done. In case of CRYPTO
 *   HW module using BUFC, BUFC must be setup prior using AESDRV.
 *
 * @param[in] config
 *   HW acceleration details. 
 *  
 * @warning
 *   AESDRV is not enabling BUFC clock and doesn't do any global BUFC init. It
 *   has to be done prior to calling AESDRV_Init function. When using DMA, it
 *   does do DMA initialisation and resource allocation, however.
 *
 * @return
 *   Error code.
 ******************************************************************************/
Ecode_t AESDRV_Init(AESDRV_Config_t*          config);

/***************************************************************************//**
 * @brief
 *   DeInitializes AESDRV. In case of AES HW module there is nothing to be
 *   done. In case of CRYPTO HW module using BUFC, BUFC must be set to its 
 *   init state.
 *
 * @return
 *   Error code.
 ******************************************************************************/
Ecode_t AESDRV_DeInit(void);

/***************************************************************************//**
 * @brief
 *   Set the AES encryption key.
 *
 * @details
 *   This functions sets up a 128 or 256 bit key to use for encryption and
 *   decryption in subsequent calls to AESDRV.
 *
 * @param[in] pAesdrvContext
 *   Pointer to AESDRV context structure.
 *
 * @param[in] pKey
 *   Pointer to buffer including the AES key.
 *
 * @param[in] keyLength
 *   The length (in bytes) of the AES key.
 *
 * @return Error code or ECODE_OK.
 ******************************************************************************/
Ecode_t AESDRV_SetKey(AESDRV_CommonContext_t* pAesdrvContext,
                      const uint8_t*          pKey,
                      uint32_t                keyLength);
  
/***************************************************************************//**
 * @brief
 *   Generate 128 bit decryption key from 128 bit encryption key. The decryption
 *   key is used for some cipher modes when decrypting.
 *
 * @details
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pAesdrvContext
 *   Pointer to AESDRV context structure.
 *
 * @param[out] pKeyOut
 *   Buffer to place 128 bit decryption key. Must be at least 16 bytes long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] pKeyIn
 *   Buffer holding 128 bit encryption key. Must be at least 16 bytes long.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_DecryptKey128(AESDRV_CommonContext_t* pAesdrvContext,
                             uint8_t*                pKeyOut,
                             const uint8_t*          pKeyIn);

/***************************************************************************//**
 * @brief
 *   Generate 256 bit decryption key from 256 bit encryption key. The decryption
 *   key is used for some cipher modes when decrypting.
 *
 * @details
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pAesdrvContext
 *   Pointer to AESDRV context structure.
 *
 * @param[out] pKeyOut
 *   Buffer to place 256 bit decryption key. Must be at least 32 bytes long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] pKeyIn
 *   Buffer holding 256 bit encryption key. Must be at least 32 bytes long.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_DecryptKey256(AESDRV_CommonContext_t* pAesdrvContext,
                             uint8_t*                pKeyOut,
                             const uint8_t *         pKeyIn);

/***************************************************************************//**
 * @brief
 *   Cipher-block chaining (CBC) cipher mode encryption/decryption, 128 bit key.
 *
 * @details
 *   Encryption:
 * @verbatim
 *           Plaintext                  Plaintext
 *               |                          |
 *               V                          V
 * InitVector ->XOR        +-------------->XOR
 *               |         |                |
 *               V         |                V
 *       +--------------+  |        +--------------+
 * Key ->| Block cipher |  |  Key ->| Block cipher |
 *       |  encryption  |  |        |  encryption  |
 *       +--------------+  |        +--------------+
 *               |---------+                |
 *               V                          V
 *           Ciphertext                 Ciphertext
 * @endverbatim
 *   Decryption:
 * @verbatim
 *         Ciphertext                 Ciphertext
 *              |----------+                |
 *              V          |                V
 *       +--------------+  |        +--------------+
 * Key ->| Block cipher |  |  Key ->| Block cipher |
 *       |  decryption  |  |        |  decryption  |
 *       +--------------+  |        +--------------+
 *               |         |                |
 *               V         |                V
 * InitVector ->XOR        +-------------->XOR
 *               |                          |
 *               V                          V
 *           Plaintext                  Plaintext
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   When doing encryption, this is the 128 bit encryption key. When doing
 *   decryption, this is the 128 bit decryption key. The decryption key may
 *   be generated from the encryption key with AESDRV_DecryptKey128().
 *   On devices supporting key buffering this argument can be null, if so, the
 *   key will not be loaded, as it is assumed the key has been loaded
 *   into KEYHA previously.
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CBC128(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );
  
/***************************************************************************//**
 * @brief
 *   Cipher-block chaining (CBC) cipher mode encryption/decryption, 256 bit key.
 *
 * @details
 *   Please see AESDRV_CBC128() for CBC figure.
 *
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   When doing encryption, this is the 256 bit encryption key. When doing
 *   decryption, this is the 256 bit decryption key. The decryption key may
 *   be generated from the encryption key with AESDRV_DecryptKey256().
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CBC256(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );
  
/***************************************************************************//**
 * @brief
 *   Cipher feedback (CFB) cipher mode encryption/decryption, 128 bit key.
 *
 * @details
 *   Encryption:
 * @verbatim
 *           InitVector    +----------------+
 *               |         |                |
 *               V         |                V
 *       +--------------+  |        +--------------+
 * Key ->| Block cipher |  |  Key ->| Block cipher |
 *       |  encryption  |  |        |  encryption  |
 *       +--------------+  |        +--------------+
 *               |         |                |
 *               V         |                V
 *  Plaintext ->XOR        |   Plaintext ->XOR
 *               |---------+                |
 *               V                          V
 *           Ciphertext                 Ciphertext
 * @endverbatim
 *   Decryption:
 * @verbatim
 *          InitVector     +----------------+
 *               |         |                |
 *               V         |                V
 *       +--------------+  |        +--------------+
 * Key ->| Block cipher |  |  Key ->| Block cipher |
 *       |  encryption  |  |        |  encryption  |
 *       +--------------+  |        +--------------+
 *               |         |                |
 *               V         |                V
 *              XOR<- Ciphertext           XOR<- Ciphertext
 *               |                          |
 *               V                          V
 *           Plaintext                  Plaintext
 * @endverbatim
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   128 bit encryption key is used for both encryption and decryption modes.
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CFB128(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );
  
/***************************************************************************//**
 * @brief
 *   Cipher feedback (CFB) cipher mode encryption/decryption, 256 bit key.
 *
 * @details
 *   Please see AESDRV_CFB128() for CFB figure.
 *
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   256 bit encryption key is used for both encryption and decryption modes.
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CFB256(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );
  
/***************************************************************************//**
 * @brief
 *   Counter (CTR) cipher mode encryption/decryption, 128 bit key.
 *
 * @details
 *   Encryption:
 * @verbatim
 *           Counter                    Counter
 *              |                          |
 *              V                          V
 *       +--------------+           +--------------+
 * Key ->| Block cipher |     Key ->| Block cipher |
 *       |  encryption  |           |  encryption  |
 *       +--------------+           +--------------+
 *              |                          |
 * Plaintext ->XOR            Plaintext ->XOR
 *              |                          |
 *              V                          V
 *         Ciphertext                 Ciphertext
 * @endverbatim
 *   Decryption:
 * @verbatim
 *           Counter                    Counter
 *              |                          |
 *              V                          V
 *       +--------------+           +--------------+
 * Key ->| Block cipher |     Key ->| Block cipher |
 *       |  encryption  |           |  encryption  |
 *       +--------------+           +--------------+
 *               |                          |
 * Ciphertext ->XOR           Ciphertext ->XOR
 *               |                          |
 *               V                          V
 *           Plaintext                  Plaintext
 * @endverbatim
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   128 bit encryption key.
 *   On devices supporting key buffering this argument can be null, if so, the
 *   key will not be loaded, as it is assumed the key has been loaded
 *   into KEYHA previously.
 *
 * @param[in,out] ctr
 *   128 bit initial counter value. The counter is updated after each AES
 *   block encoding through use of @p ctrFunc.
 *
 * @param[in] ctrCallback
 *   Callback function used to update counter value. If NULL then
 *   AES_CTRUpdate32Bit from emlib will be used.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CTR128(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      uint8_t*                     ctr,
                      AESDRV_CtrCallback_t         ctrCallback,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );
  
/***************************************************************************//**
 * @brief
 *   Counter (CTR) cipher mode encryption/decryption, 256 bit key.
 *
 * @details
 *   Please see AES_CTR128() for CTR figure.
 *
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   256 bit encryption key.
 *
 * @param[in,out] ctr
 *   128 bit initial counter value. The counter is updated after each AES
 *   block encoding through use of @p ctrFunc.
 *
 * @param[in] ctrCallback
 *   Callback function used to update counter value. If NULL then
 *   AES_CTRUpdate32Bit from emlib will be used.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_CTR256(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      uint8_t*                     ctr,
                      AESDRV_CtrCallback_t         ctrCallback,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );

/***************************************************************************//**
 * @brief
 *   Electronic Codebook (ECB) cipher mode encryption/decryption, 128 bit key.
 *
 * @details
 *   Encryption:
 * @verbatim
 *          Plaintext                  Plaintext
 *              |                          |
 *              V                          V
 *       +--------------+           +--------------+
 * Key ->| Block cipher |     Key ->| Block cipher |
 *       |  encryption  |           |  encryption  |
 *       +--------------+           +--------------+
 *              |                          |
 *              V                          V
 *         Ciphertext                 Ciphertext
 * @endverbatim
 *   Decryption:
 * @verbatim
 *         Ciphertext                 Ciphertext
 *              |                          |
 *              V                          V
 *       +--------------+           +--------------+
 * Key ->| Block cipher |     Key ->| Block cipher |
 *       |  decryption  |           |  decryption  |
 *       +--------------+           +--------------+
 *              |                          |
 *              V                          V
 *          Plaintext                  Plaintext
 * @endverbatim
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   When doing encryption, this is the 128 bit encryption key. When doing
 *   decryption, this is the 128 bit decryption key. The decryption key may
 *   be generated from the encryption key with AESDRV_DecryptKey128().
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_ECB128(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );

/***************************************************************************//**
 * @brief
 *   Electronic Codebook (ECB) cipher mode encryption/decryption, 256 bit key.
 *
 * @details
 *   Please see AESDRV_ECB128() for ECB figure.
 *
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   When doing encryption, this is the 256 bit encryption key. When doing
 *   decryption, this is the 256 bit decryption key. The decryption key may
 *   be generated from the encryption key with AESDRV_DecryptKey256().
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_ECB256(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      bool                         encrypt,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );


/***************************************************************************//**
 * @brief
 *   Output feedback (OFB) cipher mode encryption/decryption, 128 bit key.
 *
 * @details
 *   Encryption:
 * @verbatim
 *          InitVector    +----------------+
 *              |         |                |
 *              V         |                V
 *       +--------------+ |        +--------------+
 * Key ->| Block cipher | |  Key ->| Block cipher |
 *       |  encryption  | |        |  encryption  |
 *       +--------------+ |        +--------------+
 *              |         |                |
 *              |---------+                |
 *              V                          V
 * Plaintext ->XOR            Plaintext ->XOR
 *              |                          |
 *              V                          V
 *         Ciphertext                 Ciphertext
 * @endverbatim
 *   Decryption:
 * @verbatim
 *          InitVector    +----------------+
 *              |         |                |
 *              V         |                V
 *       +--------------+ |        +--------------+
 * Key ->| Block cipher | |  Key ->| Block cipher |
 *       |  encryption  | |        |  encryption  |
 *       +--------------+ |        +--------------+
 *              |         |                |
 *              |---------+                |
 *              V                          V
 * Ciphertext ->XOR           Ciphertext ->XOR
 *              |                          |
 *              V                          V
 *          Plaintext                  Plaintext
 * @endverbatim
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   128 bit encryption key.
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_OFB128(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );

/***************************************************************************//**
 * @brief
 *   Output feedback (OFB) cipher mode encryption/decryption, 256 bit key.
 *
 * @details
 *   Please see AES_OFB128() for OFB figure.
 *
 *   Please refer to general comments on layout and byte ordering of parameters.
 *
 * @param[in] pBlockCipherContext
 *   Pointer to AES block cipher context structure.
 *
 * @param[out] out
 *   Buffer to place encrypted/decrypted data. Must be at least @p len long. It
 *   may be set equal to @p in, in which case the input buffer is overwritten.
 *
 * @param[in] in
 *   Buffer holding data to encrypt/decrypt. Must be at least @p len long.
 *
 * @param[in] len
 *   Number of bytes to encrypt/decrypt. Must be a multiple of 16.
 *
 * @param[in] key
 *   256 bit encryption key.
 *
 * @param[in] iv
 *   128 bit initalization vector to use.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the AES operation and return immediately (non-blocking API). When the AES
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the AES operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return Error code
 ******************************************************************************/
Ecode_t AESDRV_OFB256(AESDRV_BlockCipherContext_t* pBlockCipherContext,
                      uint8_t*                     out,
                      const uint8_t*               in,
                      unsigned int                 len,
                      const uint8_t*               key,
                      const uint8_t*               iv,
                      AESDRV_AsynchCallback_t      asynchCallback,
                      void*                        asynchCallbackArgument
                      );

/***************************************************************************//**
 * @brief
 *   Computes the length of the MIC (Message Integrity Code)
 *   for a given security level, as defined in IEEE Std 802.15.4-2006 table 95.
 *
 * @details
 *   The two LSBs of securityLevel encodes a MIC length of 0, 4, 8, or 16.
 *
 * @param[in] securityLevel
 *   Security level to use.
 *
 * @return
 *   The length of the MIC for the given @p securityLevel
 ******************************************************************************/
uint8_t AESDRV_CCMStar_LengthOfMIC(uint8_t securityLevel);

/***************************************************************************//**
 * @brief
 *   CCM block cipher mode encryption/decryption based on 128 bit AES.
 *
 * @details
 *   Please see http://en.wikipedia.org/wiki/CCM_mode for a general description
 *   of CCM.
 *
 * @param[in] pCcmContext
 *   Pointer to CCM context structure.
 *
 * @param[in] pDataInput
 *   If @p encrypt is true, pDataInput is the 'P' (payload) parameter in CCM.
 *   I.e. the Payload data to encrypt. 
 *   If @p encrypt is false, pDataInput is the 'C' (ciphertext) parameter in CCM.
 *   I.e. the ciphertext data to decrypt. 
 *
 * @param[out] pDataOutput
 *   If @p encrypt is true, pOututData is the 'C' (ciphertext) parameter in CCM.
 *   I.e. the Ciphertext data as a result of encrypting the payload data.
 *   If @p encrypt is false, pDataOutput is the 'P' (payload) parameter in CCM.
 *   I.e. the Payload data as a result of decrypting the ciphertext.
 *
 * @param[in] dataLength
 *   Length of data to be encrypted/decrypted, referred to as 'p' in CCM.
 *   Note that this does not include the length of the MIC which is specified
 *   with @p authTagLength.
 *
 * @param[in] pHdr
 *   The 'A' parameter in CCM.
 *   Header is used for MIC calculation.
 *   Must be at least @p hdrLength long.
 *
 * @param[in] hdrLength
 *   The 'a' parameter in CCM.
 *   Length of header.
 *
 * @param[in] pKey
 *   The 'K' parameter in CCM.
 *   Pointer to key buffer. If pKey is NULL, the current key will be used.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] keyLength
 *   The length in bytes, of the @p pKey, i.e. the 'K' parameter in CCM.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] pNonce
 *   The 'N' parameter in CCM.
 *   Pointer to the nonce, which must have length 15-authTagLength
 *   See @p authTagLength
 *
 * @param[in] nonceLength
 *   The length in bytes, of the @p pNonce, i.e. the 'N' parameter in CCM.
 *   Currently only nonce size equal to 13 bytes is supported.
 *
 * @param[in/out] pAuthTag
 *   The 'MIC' parameter in CCM.
 *   Pointer to the MIC buffer, which must have length @p authTagLength.
 *
 * @param[in] authTagLength
 *   The 't' parameter in CCM.
 *   The number of bytes used for the authentication tag.
 *   Possible values are 0, 4, 6, 8, 10, 12, 14, 16.
 *   Note that 0 is not a legal value in CCM, but is used for CCM*.
 *
 * @param[in] encrypt
 *   Set to true to run the generation-encryption process,
 *   false to run the decryption-verification process.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the CCM operation and return immediately (non-blocking API). When the CCM
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the CCM operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return
 *   ECODE_OK if success. Error code if failure.
 *   Encryption will always succeed.
 *   Decryption may fail if the authentication fails.
 ******************************************************************************/
Ecode_t AESDRV_CCM(AESDRV_CCM_Context_t*   pCcmContext,
                   const uint8_t*          pDataInput,
                         uint8_t*          pDataOutput,
                   const uint32_t          dataLength,
                   const uint8_t*          pHdr,
                   const uint32_t          hdrLength,
                   const uint8_t*          pKey,
                   const uint32_t          keyLength,
                   const uint8_t*          pNonce,
                   const uint32_t          nonceLength,
                         uint8_t*          pAuthTag,
                   const uint8_t           authTagLength,
                   const bool              encrypt,
                   AESDRV_AsynchCallback_t asynchCallback,
                   void*                   asynchCallbackArgument
                   );

/***************************************************************************//**
 * @brief
 *   CCM* block cipher mode encryption/decryption based on 128 bit AES.
 *
 * @details
 *   Please see IEEE Std 802.15.4-2006 Annex B for a description of CCM*.
 *
 * @param[in] pCcmContext
 *   Pointer to CCM context structure.
 *
 * @param[in] pDataInput
 *   If @p encrypt is true, pDataInput is the plaintext.
 *   I.e. the payload data to encrypt. 
 *   If @p encrypt is false, pDataInput is the ciphertext.
 *   I.e. the ciphertext data to decrypt. 
 *
 * @param[out] pDataOutput
 *   If @p encrypt is true, pDataOutput is the ciphertext.
 *   I.e. the Ciphertext data as a result of encrypting the payload data.
 *   If @p encrypt is false, pDataOutput is the plaintext.
 *   I.e. the Payload data as a result of decrypting the ciphertext.
 *
 * @param[in] dataLength
 *   Length of data to be encrypted/decrypted, referred to as l(m) in CCM*.
 *   Note that this does not include the length of the MIC,
 *   so for decryption there are
 *   l(c) = @p dataLength + CCM_LengthOfMIC(securityLevel)
 *   bytes available in the buffer.
 *
 * @param[in] pHdr
 *   The 'a' parameter in CCM*.
 *   Header is used for MIC calculation.
 *   Must be at least @p hdrLength long.
 *
 * @param[in] hdrLength
 *   Length of header.
 *   Referred to as l(a) in CCM*
 *
 * @param[in] pKey
 *   The 'K' parameter in CCM*.
 *   Pointer to key to use. If pKey is NULL, the current key will be used.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] keyLength
 *   The length in bytes, of the @p pKey, i.e. the 'K' parameter in CCM*.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] pNonce
 *   The 'N' parameter in CCM.
 *   Pointer to the nonce, which has length 13 bytes.
 *
 * @param[in] nonceLength
 *   The length in bytes, of the @p pNonce, i.e. the 'N' parameter in CCM*.
 *
 * @param[in/out] pAuthTag
 *   The 'MIC' parameter in CCM.
 *   Pointer to the MIC buffer, which must have length @p authTagLength.
 *
 * @param[in] securityLevel
 *   Security level to use. See table 95 in IEEE Std 802.15.4-2006
 *   See also function CCM_LengthOfMIC
 *   Level 0: No encryption, no authentication
 *   Level 1: No encryption, M=4 bytes authentication tag
 *   Level 2: No encryption, M=8 bytes authentication tag
 *   Level 3: No encryption, M=16 bytes authentication tag
 *   Level 4: Encryption, no authentication
 *   Level 5: Encryption, M=4 bytes authentication tag
 *   Level 6: Encryption, M=8 bytes authentication tag
 *   Level 7: Encryption, M=16 bytes authentication tag
 *
 * @param[in] encrypt
 *   Set to true to encrypt, false to decrypt.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the CCM operation and return immediately (non-blocking API). When the CCM
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the CCM operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return
 *   ECODE_OK if success. Error code if failure.
 *   Encryption will always succeed.
 *   Decryption may fail if the authentication fails.
 ******************************************************************************/
Ecode_t AESDRV_CCMStar(AESDRV_CCM_Context_t*   pCcmContext,
                       const uint8_t*          pDataInput,
                             uint8_t*          pDataOutput,
                       const uint32_t          dataLength,
                       const uint8_t*          pHdr,
                       const uint32_t          hdrLength,
                       const uint8_t*          pKey,
                       const uint32_t          keyLength,
                       const uint8_t*          pNonce,
                       const uint32_t          nonceLength,
                             uint8_t*          pAuthTag,
                       const uint8_t           securityLevel,
                       const bool              encrypt,
                       AESDRV_AsynchCallback_t asynchCallback,
                       void*                   asynchCallbackArgument
                       );
  
/***************************************************************************//**
 * @brief
 *  CCM optimized for BLE
 *
 * @details
 *  This function is an implementation of CCM optimized for Bluetooth Low Energy
 *  (BLE). This function assumes fixed header size (1 byte),
 *  fixed authentication tag (4bytes), fixed length field size (2 bytes)
 *
 * @param[in] pCcmContext
 *   Pointer to CCM context structure.
 *
 * @param pData
 *  Pointer to data
 *
 * @param dataLength
 *  length of data (max. 27)
 *
 * @param hdr
 *  1 byte header
 *
 * @param pKey
 *  10 byte Security Key. If pKey is NULL, the current key will be used.
 *
 * @param pNonce
 *  13 byte nonce
 *
 * @param encrypt
 *  true - encrypt
 *  false - decrypt
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the CCM operation and return immediately (non-blocking API). When the CCM
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the CCM operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return
 *   ECODE_OK if success. Error code if failure.
 *   Encryption will always succeed.
 *   Decryption may fail if the authentication fails.
 */
Ecode_t AESDRV_CCMBLE(AESDRV_CCM_Context_t*   pCcmContext,
                      uint8_t*                pData,
                      const uint32_t          dataLength,
                      uint8_t                 hdr,
                      const uint8_t*          pKey,
                      const uint8_t*          pNonce,
                      const bool              encrypt,
                      AESDRV_AsynchCallback_t asynchCallback,
                      void*                   asynchCallbackArgument
                      );

/***************************************************************************//**
 * @brief
 *   GCM (Galois Counter Mode) block cipher mode encryption/decryption based
 *   on 128 bit AES.
 *
 * @details
 *   Please seehttp://en.wikipedia.org/wiki/Galois/Counter_Mode for a general
 *   description of GCM, or for
 *   http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-spec.pdf
 *   for detailed specification.
 *
 *   This is generic function which based on internal AESDRV settings is
 *   using HW acceleration in data path or not. Function expects that
 *   pHdr and pData are consecutive.
 *
 * @param[in] pGcmContext
 *   Pointer to GCM context structure.
 *
 * @param[in] pDataInput
 *   If @p encrypt is true, pDataInput is the plaintext.
 *   I.e. the payload data to encrypt. 
 *   If @p encrypt is false, pDataInput is the ciphertext.
 *   I.e. the ciphertext data to decrypt. 
 *
 * @param[out] pDataOutput
 *   If @p encrypt is true, pDataOutput is the ciphertext.
 *   I.e. the Ciphertext data as a result of encrypting the payload data.
 *   If @p encrypt is false, pDataOutput is the plaintext.
 *   I.e. the Payload data as a result of decrypting the ciphertext.
 *
 * @param[in] dataLength
 *   Length of plaintext to be encrypted, referred to as 'n' in GCM.
 *   Note that this does not include the length of the MIC,
 *
 * @param[in] pHdr
 *   The 'A' parameter in GCM.
 *   Header is used for MIC calculation.
 *   Must be @p hdrLength long.
 *
 * @param[in] hdrLength
 *   The 'm' parameter in GCM.
 *   Length of authentication data.
 *
 * @param[in] pKey
 *   The 'K' parameter in GCM.
 *   Pointer to key buffer. If pKey is NULL, the current key will be used.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] keyLength
 *   The length in bytes, of the @p pKey, i.e. the 'K' parameter in CCM.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] pInitialVector
 *   The 'IV' parameter in GCM.
 *   Pointer to the initial vector, which must have length 12 bytes (=96 bits
 *   which is recommended by GCM for efficiency).
 *
 * @param[in/out] pAuthTag
 *   The 'MIC' parameter in CCM.
 *   Pointer to the MIC buffer, which must have length @p authTagLength.
 *
 * @param[in] authTagLength
 *  Length of authentication tag 0-16 bytes.
 *
 * @param[in] encrypt
 *   Set to true to run the generation-encryption process,
 *   false to run the decryption-verification process.
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the GCM operation and return immediately (non-blocking API). When the GCM
 *   operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the GCM operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return
 *   ECODE_OK if success. Error code if failure.
 *   Encryption will always succeed.
 *   Decryption may fail if the authentication fails.
 ******************************************************************************/
Ecode_t AESDRV_GCM(AESDRV_GCM_Context_t*   pGcmContext,
                   const uint8_t*          pDataInput,
                         uint8_t*          pDataOutput,
                   const uint32_t          dataLength,
                   const uint8_t*          pHdr,
                   const uint32_t          hdrLength,
                   const uint8_t*          pKey,
                   const uint32_t          keyLength,
                   const uint8_t*          pInitialVector,
                   const uint32_t          initialVectorLength,
                   uint8_t*                pAuthTag,
                   const uint8_t           authTagLength,
                   const bool              encrypt,
                   AESDRV_AsynchCallback_t asynchCallback,
                   void*                   asynchCallbackArgument);

/***************************************************************************//**
 * @brief
 *  Function is an implementation of CMAC-AES128
 * @details
 *  Function assumes fixed key length of 128bit, digest of max 128bit.
 *
 * @param[in] pCmacContext
 *  Pointer to CMAC context structure.
 *
 * @param[in] pData
 *  Pointer to data (message) Be careful: this memory should be allocated on
 *  block-size (128-bit) boundaries!
 *
 * @param[in] dataLengthBits
 *  length of actual data in bits
 *
 * @param[in] pKey
 *  Pointer to key buffer for the AES algorithm.
 *  If pKey is NULL, the current key will be used.
 *  Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in] keyLength
 *   The length in bytes, of the @p pKey, i.e. the 'K' parameter in CCM.
 *   Currently only 128 bit keys (16 bytes) are supported.
 *
 * @param[in/out] pDigest
 *  128-bit (maximum) digest. If encrypting, the digest will be stored there.
 *  If verifying, the calculated digest will be compared to the one stored in
 *  this place.
 *  Warning: regardless of digestLengthBits, 128 bits will get written here.
 *
 * @param[in] digestLengthBits
 *  Requested length of the message digest in bits. LSB's will be zeroed out.
 *
 * @param[in] encrypt
 *  true - Generate hash
 *  false - Verify hash
 *
 * @param[in]  asynchCallback
 *   If non-NULL, this function will operate in asynchronous mode by starting
 *   the CMAC operation and return immediately (non-blocking API). When the
 *   CMAC operation has completed, the ascynchCallback function will be called.
 *   If NULL, this function will operate in synchronous mode, and block until
 *   the CMAC operation has completed.
 *
 * @param[in]  asynchCallbackArgument
 *   User defined parameter to be sent to callback function.
 *
 * @return
 *   ECODE_OK if success. Error code if failure.
 *   Encryption will always succeed.
 *   Decryption may fail if the authentication fails.
 */
Ecode_t AESDRV_CMAC(AESDRV_CMAC_Context_t*  pCmacContext,
                    uint8_t*                pData,
                    uint32_t                dataLengthBits,
                    const uint8_t*          pKey,
                    const uint32_t          keyLength,
                    uint8_t*                pDigest,
                    uint16_t                digestLengthBits,
                    const bool              encrypt,
                    AESDRV_AsynchCallback_t asynchCallback,
                    void*                   asynchCallbackArgument
                    );

/** @} (end addtogroup AESDRV) */
/** @} (end addtogroup CRYPTOLIB) */

#ifdef __cplusplus
}
#endif

#endif /* #if ( defined(CRYPTO_COUNT) && (CRYPTO_COUNT > 0) ) || \
          ( defined(AES_COUNT) && (AES_COUNT > 0) ) */

#endif /* __SILICON_LABS_AESDRV_H */
