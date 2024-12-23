/*
    crypt.h -- Crypt header.

    The crypt library provides a minimal set of crypto for connected devices.
    It provides Base64 encode/decode, MD5, SHA256, Bcrypt crypto and password utilities.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#pragma once

#ifndef _h_CRYPT
#define _h_CRYPT 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"

/**
    Minimal Crypto Library
    @description The crypt library provides a minimal set of crypto for connected devices.
        It provides Base64 encode/decode, MD5, SHA256, Bcrypt crypto and password utilities.
    @defgroup Crypt Crypt
    @stability Evolving
 */

/*********************************** Defines **********************************/

/**
    Minimal crypto library offering Base64, MD5, SHA256 and Bcrypt services.
    @defgroup Crypt Crypt
    @stability Evolving
*/

#ifndef ME_COM_CRYPT
    #define ME_COM_CRYPT 1
#endif
#ifndef ME_CRYPT_MAX_PASSWORD
    #define ME_CRYPT_MAX_PASSWORD 64        /** Maximum password length */
#endif
#ifndef ME_CRYPT_MD5
    #define ME_CRYPT_MD5          0
#endif
#ifndef ME_CRYPT_SHA256
    #define ME_CRYPT_SHA256       1
#endif
#ifndef ME_CRYPT_BCRYPT
    #define ME_CRYPT_BCRYPT       1
#endif
#ifndef ME_CRYPT_MBEDTLS
    #define ME_CRYPT_MBEDTLS      0
#endif
#if ME_CRYPT_BASE64 || ME_CRYPT_BCRYPT
    #define ME_CRYPT_BASE64       1
#endif

/*********************************** Base-64 **********************************/

#if ME_CRYPT_BASE64 || DOXYGEN

#define CRYPT_DECODE_TOKEQ 1                 /**< Decode base64 blocks up to a NULL or equals */

/**
    Encode a string using base64 encoding
    @param str Null terminated string to encode
    @return Base64 encoded string. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptEncode64Block
*/
PUBLIC char *cryptEncode64(cchar *str);

/**
    Encode a block using base64 encoding
    @param block Block of data to encode
    @param len Length of the block
    @return Base64 encoded string. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptEncode64
*/
PUBLIC char *cryptEncode64Block(cchar *block, ssize len);

/**
    Decode a block that has been base64 encoded
    @param str Base64 encoded string
    @return Null terminated decoded string. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptEncode64
*/
PUBLIC char *cryptDecode64(cchar *str);

/**
    Decode a block that has been base64 encoded
    @param block Base64 encoded string
    @param len Pointer to receive the length of the decoded block.
    @param flags Stop decoding at the end of the block or '=' if CRYPT_DECODE_TOKEQ is specified.
    @return Decoded block string. Caller must free. The length is described via *len.
    @stability Evolving
    @ingroup Crypt
    @see cryptDecode64, cryptEncode64, cryptEncode64Block
*/
PUBLIC char *cryptDecode64Block(cchar *block, ssize *len, int flags);
#endif

/************************************* MD5 ************************************/

#if ME_CRYPT_MD5 || DOXYGEN
#define CRYPT_MD5_SIZE 16

/**
    MD5 computation block
    @ingroup Crypt
    @stability Internal
*/
typedef struct RMd5 {
    uint state[4];              /**< MD5 hashing state */
    uint count[2];
    uchar buffer[64];
} RMd5;

/**
    Get an MD5 hash for a block and return a binary hash.
    @param block Block of data for which to compute the hash.
    @param length Length of the block. If the length is -1, the block is assumed to be a string
        and its length is determined by strlen on the block.
    @param hash Array to receive the hash
    @stability Evolving
    @ingroup Crypt
    @see cryptGetMd5
*/
PUBLIC void cryptGetMd5Block(uchar *block, ssize length, uchar hash[CRYPT_MD5_SIZE]);

/**
    Get an MD5 hash for a block and return a string hash.
    @param block Block of data for which to compute the hash.
    @param length Length of the block. If the length is -1, the block is assumed to be a string
        and its length is determined by strlen on the block.
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetMd5Block
*/
PUBLIC char *cryptGetMd5(uchar *block, ssize length);

/**
    Get an MD5 string hash for a file.
    @param path Filename for the file for which to compute the hash.
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetMd5, cryptGetMd5Block
*/
PUBLIC char *cryptGetFileMd5(cchar *path);

/**
    Convert an MD5 hash to a hex string.
    @param hash Previously computed MD5 hash.
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetMd5, cryptGetMd5Block
*/
PUBLIC char *cryptMd5HashToString(uchar hash[CRYPT_MD5_SIZE]);

/**
    Low level MD5 hashing API to initialize an MD5 hash computation.
    @description Initialize the hash computation
    @param ctx MD5 context
    @stability Evolving
    @ingroup Crypt
    @see cryptMd5Update, cryptMd5Finalize
*/
PUBLIC void cryptMd5Init(RMd5 *ctx);

/**
    Low level MD5 hashing API to update an MD5 hash computation with a block of data.
    @description Update the hash computation with input.
    @param ctx MD5 context
    @param block Input block to add to the hash
    @param length Length of the input block.
    @stability Evolving
    @ingroup Crypt
    @see cryptMd5Init, cryptMd5Finalize
*/

PUBLIC void cryptMd5Update(RMd5 *ctx, uchar *block, uint length);

/**
    Low level MD5 hashing API to finalize an MD5 hash compuation and return a binary hash result.
    @description Finalize the hash computation
    @param ctx MD5 context
    @param digest MD5 array to receive the hash result.
    @stability Evolving
    @ingroup Crypt
    @see cryptMd5Init, cryptMd5Update
*/
PUBLIC void cryptMd5Finalize(RMd5 *ctx, uchar digest[CRYPT_MD5_SIZE])
#endif

/*********************************** SHA256 ***********************************/

#if ME_CRYPT_SHA256 || DOXYGEN

#define CRYPT_SHA256_SIZE 32

/**
    SHA256 computation block
    @ingroup Crypt
    @stability Internal
*/
typedef struct RSha256 {
    uint32 count[2];
    uint32 state[8];            /**< SHA256 computation state */
    uchar buffer[64];
} RSha256;

/**
    Get a SHA256 hash for a block and return a binary hash.
    @param block Block of data for which to compute the hash.
    @param length Length of the data block.
        If set to -1, the block is assumed to be a null terminated string.
    @param hash Array to receive the hash result.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetSha256, cryptGetFileSha256
*/
PUBLIC void cryptGetSha256Block(cuchar *block, ssize length, uchar hash[CRYPT_SHA256_SIZE]);

/**
    Get a SHA256 hash for a block and return a string hash.
    @param block Block of data for which to compute the hash.
        If set to -1, the block is assumed to be a null terminated string.
    @param length Length of the data block.
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetSha256
*/
PUBLIC char *cryptGetSha256(cuchar *block, ssize length);

/**
    Get a SHA256 hash for the contents of a file.
    @param path Filename of the file
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetSha256, cryptGetSha256Block
*/
PUBLIC char *cryptGetFileSha256(cchar *path);

/**
    Convert a SHA256 hash to a string
    @param hash Hash result from #cryptGetSha256Block
    @return A hex string representation of the hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetSha256, cryptGetSha256Block
*/
PUBLIC char *cryptSha256HashToString(uchar hash[CRYPT_SHA256_SIZE]);

/**
    Low level SHA256 hashing API to initialize a SHA256 hash computation.
    @description Initialize the hash computation
    @param ctx SHA256 context
    @stability Evolving
    @ingroup Crypt
    @see cryptSha256Finalize, cryptSha256Start, cryptSha256Update
*/
PUBLIC void cryptSha256Init(RSha256 *ctx);

/**
    Low level SHA256 hashing API to terminate a SHA256 hash compuation.
    @description Terminate (conclude) the hash computation.
        This erases in-memory state and should be the final step in computing a hash.
    @param ctx SHA256 context
    @stability Evolving
    @ingroup Crypt
    @see cryptSha256Init, cryptSha256Finalize, cryptSha256Start, cryptSha256Update
*/
PUBLIC void cryptSha256Term(RSha256 *ctx);

/**
    Low level SHA256 hashing API to finalize a SHA256 hash compuation and return a binary result.
    @description Finalize the hash computation and return a binary hash result.
    @param ctx SHA256 context
    @param hash Array to receive the hash result.
    @stability Evolving
    @ingroup Crypt
    @see cryptSha256Init, cryptSha256Start, cryptSha256Term, cryptSha256Update
*/
PUBLIC void cryptSha256Finalize(RSha256 *ctx, uchar hash[CRYPT_SHA256_SIZE]);

/**
    Low level SHA256 hashing API to start a SHA256 hash computation.
    @description Start the hash computation.
    @param ctx SHA256 context
    @stability Evolving
    @ingroup Crypt
    @see cryptSha256Finalize, cryptSha256Init, cryptSha256Start, cryptSha256Term, cryptSha256Update
*/
PUBLIC void cryptSha256Start(RSha256 *ctx);

/**
    Low level SHA256 hashing API to update a SHA256 hash computation with input data.
    @description Update the hash computation with a block of data.
    @param ctx SHA256 context
    @param block Block of data to hash
    @param length Length of the input block.
    @stability Evolving
    @ingroup Crypt
    @see cryptSha256Finalize, cryptSha256Init, cryptSha256Start, cryptSha256Term
*/
PUBLIC void cryptSha256Update(RSha256 *ctx, cuchar *block, int length);
#endif

/*********************************** bcrypt ***********************************/

#if ME_CRYPT_BCRYPT || DOXYGEN

#define CRYPT_BLOWFISH             "BF1"        /**< Blowfish hash tag */
#define CRYPT_BLOWFISH_SALT_LENGTH 16           /**< Length of salt text */
#define CRYPT_BLOWFISH_ROUNDS      128          /**< Number of computation rounds */

/**
    Make a password using the Blowfish cipher (Bcrypt)
    @param password Input plain-text password
    @param saltLength Length of salt text to add
    @param rounds Number of computation rounds. Default is 128. Longer is slower, but more secure.
    @return The computed password hash. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptGetPassword, cryptCheckPassword
*/
PUBLIC char *cryptMakePassword(cchar *password, int saltLength, int rounds);

/**
    Check a plain-text password against a password hash.
    @param plainTextPassword Input plain-text password
    @param passwordHash Hash previously computed via #cryptMakePassword
    @return True if the password matches
    @stability Evolving
    @ingroup Crypt
    @see cryptGetPassword, cryptMakePassword
*/
PUBLIC bool cryptCheckPassword(cchar *plainTextPassword, cchar *passwordHash);
#endif

/**
    Read a password from the console
    @description Used by utility programs to read passwords from the console.
    @param prompt Password user prompt
    @return The input password. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptMakePassword
*/
PUBLIC char *cryptGetPassword(cchar *prompt);

/**
    Get random data
    @param buf Result buffer to hold the random data
    @param length Size of the buffer
    @param block Set to true to read from a blocking random generator that will guarantee
        the return of random data in the situation of insufficient entropy at the time the call
        was made.
    @return The input password. Caller must free.
    @stability Evolving
    @ingroup Crypt
    @see cryptMakePassword
*/
PUBLIC int cryptGetRandomBytes(char *buf, ssize length, bool block);

/******************************* MBedTLS Wrappers *****************************/

#if ME_CRYPT_MBEDTLS || DOXYGEN
/*
    These APIs require MbedTLS and are currently internal only
*/
typedef void RKey;

PUBLIC void cryptFreeKey(RKey *skey);
PUBLIC int cryptGenKey(RKey *skey);
PUBLIC int cryptGetPubKey(RKey *skey, uchar *buf, ssize bufsize);
PUBLIC int cryptLoadPubKey(RKey *skey, uchar *buf, ssize bufsize);
PUBLIC RKey *cryptParsePubKey(RKey *skey, cchar *buf, ssize buflen);
PUBLIC int cryptSign(RKey *skey, uchar *sum, ssize sumsize);
PUBLIC RKey *cryptParsePubKey(RKey *skey, cchar *buf, ssize buflen);
PUBLIC int cryptVerify(RKey *skey, uchar *sum, ssize sumsize, uchar *signature, ssize siglen);
#endif

//  DOC
PUBLIC char *cryptID(ssize size);

#ifdef __cplusplus
}
#endif

#endif /* _h_CRYPT */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */