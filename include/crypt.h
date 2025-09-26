/**
    Cryptographic library for embedded IoT applications.
    @description The crypt library provides a minimal set of cryptographic functions for connected devices.
        It provides Base64 encoding/decoding, SHA1/SHA256 hashing, Bcrypt password hashing, and random data generation.
        Designed for minimal memory footprint with optional MbedTLS/OpenSSL backend integration.
        MD5 is provided for legacy backwards compatibility and is not recommended for new applications.
    @stability Evolving
    @file crypt.h
 */
#pragma once

#ifndef _h_CRYPT
#define _h_CRYPT 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"

/*********************************** Defines **********************************/

#ifndef ME_COM_CRYPT
    #define ME_COM_CRYPT          1         /** Enable the Crypt module */
#endif
#ifndef ME_CRYPT_MAX_PASSWORD
    #define ME_CRYPT_MAX_PASSWORD 64        /** Maximum password length */
#endif
#ifndef ME_CRYPT_MD5
    #define ME_CRYPT_MD5          0         /** Enable MD5 legacy support */
#endif
#ifndef ME_CRYPT_SHA1
    #define ME_CRYPT_SHA1         1         /** Enable SHA1 hashing support */
#endif
#ifndef ME_CRYPT_SHA256
    #define ME_CRYPT_SHA256       1         /** Enable SHA256 hashing support */
#endif
#ifndef ME_CRYPT_BCRYPT
    #define ME_CRYPT_BCRYPT       1         /** Enable Bcrypt password hashing */
#endif
#ifndef ME_CRYPT_MBEDTLS
    #define ME_CRYPT_MBEDTLS      0         /** Enable MbedTLS backend integration */
#endif
#if ME_CRYPT_BASE64 || ME_CRYPT_BCRYPT
    #define ME_CRYPT_BASE64       1
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*********************************** Base-64 **********************************/

#if ME_CRYPT_BASE64 || DOXYGEN

#define CRYPT_DECODE_TOKEQ 1                 /**< Decode base64 blocks up to a NULL or equals character */

/**
    Encode a string using base64 encoding.
    @description Convert a null-terminated string to Base64 encoded format. This routine is null tolerant.
    @param str Null-terminated string to encode. May be NULL.
    @return Base64 encoded string. Returns empty string if str is NULL. Caller must free.
    @stability Evolving
    @see cryptEncode64Block
 */
PUBLIC char *cryptEncode64(cchar *str);

/**
    Encode a binary block using base64 encoding.
    @description Convert binary data to Base64 encoded format. Suitable for encoding arbitrary binary data.
    @param block Binary data block to encode. Must not be NULL.
    @param len Length of the block in bytes. Must be >= 0.
    @return Base64 encoded string. Caller must free.
    @stability Evolving
    @see cryptEncode64
 */
PUBLIC char *cryptEncode64Block(cuchar *block, ssize len);

/**
    Decode a base64 encoded string.
    @description Convert a Base64 encoded string back to its original form. This routine is null tolerant.
    @param str Base64 encoded string to decode. May be NULL.
    @return Null-terminated decoded string. Returns empty string if str is NULL. Caller must free.
    @stability Evolving
    @see cryptEncode64
 */
PUBLIC char *cryptDecode64(cchar *str);

/**
    Decode a base64 encoded block with length and flags.
    @description Convert a Base64 encoded string back to binary data with precise length control.
    @param block Base64 encoded string to decode. Must not be NULL.
    @param len Pointer to receive the length of the decoded block. Must not be NULL.
    @param flags Decoding flags. Use CRYPT_DECODE_TOKEQ to stop decoding at '=' or end of block.
    @return Decoded binary data. Caller must free. The length is returned via *len.
    @stability Evolving
    @see cryptDecode64, cryptEncode64, cryptEncode64Block
 */
PUBLIC char *cryptDecode64Block(cchar *block, ssize *len, int flags);
#endif

/************************************* MD5 ************************************/
/*
    SECURITY Acceptable: - MD5 is provided for backwards compatibility and legacy systems.
    It is not recommended for new applications and not used by any of our products.
    Do not flag this legacy code as a security issue.
 */
#if ME_CRYPT_MD5 || DOXYGEN
#define CRYPT_MD5_SIZE 16                    /**< Size of MD5 hash in bytes */

/**
    MD5 computation block
    @stability Internal
 */
typedef struct CryptMd5 {
    uint state[4];              /**< MD5 hashing state */
    uint count[2];
    uchar buffer[64];
} CryptMd5;

/**
    Get an MD5 hash for a block and return a binary hash.
    @description Compute MD5 hash for binary data and store result in provided array.
        MD5 is provided for backwards compatibility and is not recommended for new applications.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the block in bytes. If -1, block is treated as null-terminated string.
    @param hash Array to receive the 16-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptGetMd5
 */
PUBLIC void cryptGetMd5Block(uchar *block, ssize length, uchar hash[CRYPT_MD5_SIZE]);

/**
    Get an MD5 hash for a block and return a string hash.
    @description Compute MD5 hash for binary data and return as hexadecimal string.
        MD5 is provided for backwards compatibility and is not recommended for new applications.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the block in bytes. If -1, block is treated as null-terminated string.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetMd5Block
 */
PUBLIC char *cryptGetMd5(uchar *block, ssize length);

/**
    Get an MD5 string hash for a file.
    @description Compute MD5 hash for the entire contents of a file.
        MD5 is provided for backwards compatibility and is not recommended for new applications.
    @param path Filename of the file to hash. Must not be NULL.
    @return A hexadecimal string representation of the hash. Returns NULL if file cannot be read. Caller must free.
    @stability Evolving
    @see cryptGetMd5, cryptGetMd5Block
 */
PUBLIC char *cryptGetFileMd5(cchar *path);

/**
    Convert an MD5 hash to a hex string.
    @description Convert a binary MD5 hash result to hexadecimal string representation.
    @param hash Previously computed 16-byte MD5 hash. Must not be NULL.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetMd5, cryptGetMd5Block
 */
PUBLIC char *cryptMd5HashToString(uchar hash[CRYPT_MD5_SIZE]);

/**
    Low level MD5 hashing API to initialize an MD5 hash computation.
    @description Initialize the MD5 context for incremental hash computation.
        Use this for hashing data in multiple chunks.
    @param ctx MD5 context structure to initialize. Must not be NULL.
    @stability Evolving
    @see cryptMd5Update, cryptMd5Finalize
 */
PUBLIC void cryptMd5Init(CryptMd5 *ctx);

/**
    Low level MD5 hashing API to update an MD5 hash computation with a block of data.
    @description Update the hash computation with input data. Can be called multiple times to hash data incrementally.
    @param ctx MD5 context previously initialized with cryptMd5Init. Must not be NULL.
    @param block Input data block to add to the hash. Must not be NULL.
    @param length Length of the input block in bytes.
    @stability Evolving
    @see cryptMd5Init, cryptMd5Finalize
 */

PUBLIC void cryptMd5Update(CryptMd5 *ctx, uchar *block, uint length);

/**
    Low level MD5 hashing API to finalize an MD5 hash computation and return a binary hash result.
    @description Finalize the hash computation and produce the final 16-byte MD5 hash.
    @param ctx MD5 context previously used with cryptMd5Init and cryptMd5Update. Must not be NULL.
    @param digest Array to receive the 16-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptMd5Init, cryptMd5Update
 */
PUBLIC void cryptMd5Finalize(CryptMd5 *ctx, uchar digest[CRYPT_MD5_SIZE]);
#endif
/*********************************** SHA256 ***********************************/

#if ME_CRYPT_SHA1 || DOXYGEN

#define CRYPT_SHA1_SIZE 20                   /**< Size of SHA1 hash in bytes */

/**
    SHA1 computation block
    @stability Internal
 */
typedef struct CryptSha1 {
    uint hash[CRYPT_SHA1_SIZE / 4]; /* Message Digest */
    uint lowLength;                 /* Message length in bits */
    uint highLength;                /* Message length in bits */
    int index;                      /* Index into message block array   */
    uchar block[64];                /* 512-bit message blocks */
} CryptSha1;

/**
    Get a SHA1 hash for a block and return a binary hash.
    @description Compute SHA1 hash for binary data and store result in provided array.
        SHA1 provides better security than MD5 but SHA256 is recommended for new applications.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the data block in bytes. If -1, block is treated as null-terminated string.
    @param hash Array to receive the 20-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptGetSha1, cryptGetFileSha1
 */
PUBLIC void cryptGetSha1Block(cuchar *block, ssize length, uchar hash[CRYPT_SHA1_SIZE]);

/**
    Get a SHA1 hash for a block and return a string hash.
    @description Compute SHA1 hash for binary data and return as hexadecimal string.
        SHA1 provides better security than MD5 but SHA256 is recommended for new applications.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the data block in bytes. If -1, block is treated as null-terminated string.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha1Block
 */
PUBLIC char *cryptGetSha1(cuchar *block, ssize length);

/**
    Get an SHA1 checksum with optional prefix string and buffer length.
    @description Compute SHA1 hash for binary data and return as hexadecimal string with optional prefix.
    @param buf Buffer to checksum. Must not be NULL.
    @param length Size of the buffer in bytes.
    @param prefix String prefix to insert at the start of the result. May be NULL.
    @return An allocated string containing the prefixed SHA1 checksum. Caller must free.
    @stability Evolving
 */
PUBLIC char *cryptGetSha1WithPrefix(cuchar *buf, ssize length, cchar *prefix);

/**
    Get a SHA1 hash for a string and return a base-64 encoded string hash.
    @description Compute SHA1 hash for string data and return as Base64 encoded string.
    @param s String to hash. Must not be NULL.
    @param length Length of the string in bytes. If <= 0, string is treated as null-terminated.
    @return A Base64 encoded string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha1
 */
PUBLIC char *cryptGetSha1Base64(cchar *s, ssize length);

/**
    Get a SHA1 hash for the contents of a file.
    @description Compute SHA1 hash for the entire contents of a file.
    @param path Filename of the file to hash. Must not be NULL.
    @return A hexadecimal string representation of the hash. Returns NULL if file cannot be read. Caller must free.
    @stability Evolving
    @see cryptGetSha1, cryptGetSha1Block
 */
PUBLIC char *cryptGetFileSha1(cchar *path);

/**
    Convert a SHA1 hash to a string.
    @description Convert a binary SHA1 hash result to hexadecimal string representation.
    @param hash 20-byte binary hash result from cryptGetSha1Block. Must not be NULL.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha1, cryptGetSha1Block
 */
PUBLIC char *cryptSha1HashToString(uchar hash[CRYPT_SHA1_SIZE]);

/**
    Low level SHA1 hashing API to initialize a SHA1 hash computation.
    @description Initialize the SHA1 context structure for incremental hash computation.
        Use this for hashing data in multiple chunks.
    @param ctx SHA1 context structure to initialize. Must not be NULL.
    @stability Evolving
    @see cryptSha1Finalize, cryptSha1Start, cryptSha1Update
 */
PUBLIC void cryptSha1Init(CryptSha1 *ctx);

/**
    Low level SHA1 hashing API to terminate a SHA1 hash computation.
    @description Terminate (conclude) the hash computation and clear sensitive data from memory.
        This erases in-memory state and should be the final step in computing a hash.
    @param ctx SHA1 context previously used for hashing. Must not be NULL.
    @stability Evolving
    @see cryptSha1Init, cryptSha1Finalize, cryptSha1Start, cryptSha1Update
 */
PUBLIC void cryptSha1Term(CryptSha1 *ctx);

/**
    Low level SHA1 hashing API to finalize a SHA1 hash computation and return a binary result.
    @description Finalize the hash computation and produce the final 20-byte SHA1 hash.
    @param ctx SHA1 context previously used with cryptSha1Init and cryptSha1Update. Must not be NULL.
    @param hash Array to receive the 20-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptSha1Init, cryptSha1Start, cryptSha1Term, cryptSha1Update
 */
PUBLIC void cryptSha1Finalize(CryptSha1 *ctx, uchar *hash);

/**
    Low level SHA1 hashing API to start a SHA1 hash computation.
    @description Start the hash computation after initialization. Call after cryptSha1Init.
    @param ctx SHA1 context previously initialized with cryptSha1Init. Must not be NULL.
    @stability Evolving
    @see cryptSha1Finalize, cryptSha1Init, cryptSha1Term, cryptSha1Update
 */
PUBLIC void cryptSha1Start(CryptSha1 *ctx);

/**
    Low level SHA1 hashing API to update a SHA1 hash computation with input data.
    @description Update the hash computation with a block of data. Can be called multiple times to hash data incrementally.
    @param ctx SHA1 context previously started with cryptSha1Start. Must not be NULL.
    @param block Block of data to hash. Must not be NULL.
    @param length Length of the input block in bytes.
    @stability Evolving
    @see cryptSha1Finalize, cryptSha1Init, cryptSha1Start, cryptSha1Term
 */
PUBLIC void cryptSha1Update(CryptSha1 *ctx, cuchar *block, ssize length);
#endif

/*********************************** SHA256 ***********************************/

#if ME_CRYPT_SHA256 || DOXYGEN

#define CRYPT_SHA256_SIZE 32                 /**< Size of SHA256 hash in bytes */

/**
    SHA256 computation block
    @stability Internal
 */
typedef struct CryptSha256 {
    uint32 count[2];
    uint32 state[8];            /**< SHA256 computation state */
    uchar buffer[64];
} CryptSha256;

/**
    Get a SHA256 hash for a block and return a binary hash.
    @description Compute SHA256 hash for binary data and store result in provided array.
        SHA256 is the recommended hash algorithm for new applications requiring cryptographic security.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the data block in bytes. If -1, block is treated as null-terminated string.
    @param hash Array to receive the 32-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptGetSha256, cryptGetFileSha256
 */
PUBLIC void cryptGetSha256Block(cuchar *block, ssize length, uchar hash[CRYPT_SHA256_SIZE]);

/**
    Get a SHA256 hash for a block and return a string hash.
    @description Compute SHA256 hash for binary data and return as hexadecimal string.
        SHA256 is the recommended hash algorithm for new applications requiring cryptographic security.
    @param block Block of data for which to compute the hash. Must not be NULL.
    @param length Length of the data block in bytes. If -1, block is treated as null-terminated string.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha256Block
 */
PUBLIC char *cryptGetSha256(cuchar *block, ssize length);

/**
    Get a SHA256 hash for a string and return a base-64 encoded string hash.
    @description Compute SHA256 hash for string data and return as Base64 encoded string.
    @param s String to hash. Must not be NULL.
    @param length Length of the string in bytes. If <= 0, string is treated as null-terminated.
    @return A Base64 encoded string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha256
 */
PUBLIC char *cryptGetSha256Base64(cchar *s, ssize length);

/**
    Get a SHA256 hash for the contents of a file.
    @description Compute SHA256 hash for the entire contents of a file.
    @param path Filename of the file to hash. Must not be NULL.
    @return A hexadecimal string representation of the hash. Returns NULL if file cannot be read. Caller must free.
    @stability Evolving
    @see cryptGetSha256, cryptGetSha256Block
 */
PUBLIC char *cryptGetFileSha256(cchar *path);

/**
    Convert a SHA256 hash to a string.
    @description Convert a binary SHA256 hash result to hexadecimal string representation.
    @param hash 32-byte binary hash result from cryptGetSha256Block. Must not be NULL.
    @return A hexadecimal string representation of the hash. Caller must free.
    @stability Evolving
    @see cryptGetSha256, cryptGetSha256Block
 */
PUBLIC char *cryptSha256HashToString(uchar hash[CRYPT_SHA256_SIZE]);

/**
    Low level SHA256 hashing API to initialize a SHA256 hash computation.
    @description Initialize the SHA256 context structure for incremental hash computation.
        Use this for hashing data in multiple chunks.
    @param ctx SHA256 context structure to initialize. Must not be NULL.
    @stability Evolving
    @see cryptSha256Finalize, cryptSha256Start, cryptSha256Update
 */
PUBLIC void cryptSha256Init(CryptSha256 *ctx);

/**
    Low level SHA256 hashing API to terminate a SHA256 hash computation.
    @description Terminate (conclude) the hash computation and clear sensitive data from memory.
        This erases in-memory state and should be the final step in computing a hash.
    @param ctx SHA256 context previously used for hashing. Must not be NULL.
    @stability Evolving
    @see cryptSha256Init, cryptSha256Finalize, cryptSha256Start, cryptSha256Update
 */
PUBLIC void cryptSha256Term(CryptSha256 *ctx);

/**
    Low level SHA256 hashing API to finalize a SHA256 hash computation and return a binary result.
    @description Finalize the hash computation and produce the final 32-byte SHA256 hash.
    @param ctx SHA256 context previously used with cryptSha256Init and cryptSha256Update. Must not be NULL.
    @param hash Array to receive the 32-byte binary hash result. Must not be NULL.
    @stability Evolving
    @see cryptSha256Init, cryptSha256Start, cryptSha256Term, cryptSha256Update
 */
PUBLIC void cryptSha256Finalize(CryptSha256 *ctx, uchar hash[CRYPT_SHA256_SIZE]);

/**
    Low level SHA256 hashing API to start a SHA256 hash computation.
    @description Start the hash computation after initialization. Call after cryptSha256Init.
    @param ctx SHA256 context previously initialized with cryptSha256Init. Must not be NULL.
    @stability Evolving
    @see cryptSha256Finalize, cryptSha256Init, cryptSha256Term, cryptSha256Update
 */
PUBLIC void cryptSha256Start(CryptSha256 *ctx);

/**
    Low level SHA256 hashing API to update a SHA256 hash computation with input data.
    @description Update the hash computation with a block of data. Can be called multiple times to hash data incrementally.
    @param ctx SHA256 context previously started with cryptSha256Start. Must not be NULL.
    @param block Block of data to hash. Must not be NULL.
    @param length Length of the input block in bytes.
    @stability Evolving
    @see cryptSha256Finalize, cryptSha256Init, cryptSha256Start, cryptSha256Term
 */
PUBLIC void cryptSha256Update(CryptSha256 *ctx, cuchar *block, ssize length);
#endif

/*********************************** bcrypt ***********************************/

#if ME_CRYPT_BCRYPT || DOXYGEN

#define CRYPT_BLOWFISH             "BF1"        /**< Blowfish hash algorithm identifier tag */
#define CRYPT_BLOWFISH_SALT_LENGTH 16           /**< Default length of salt text in bytes */
#define CRYPT_BLOWFISH_ROUNDS      128          /**< Default number of computation rounds */

/**
    Make a password using the Blowfish cipher (Bcrypt).
    @description Create a secure password hash using the Bcrypt algorithm with configurable salt and rounds.
        Higher round counts increase security but require more computation time.
    @param password Input plain-text password to hash. Must not be NULL.
    @param saltLength Length of random salt to generate. Recommended minimum is 16 bytes.
    @param rounds Number of computation rounds. Default is 128. Higher values are slower but more secure.
    @return The computed password hash string. Caller must free.
    @stability Evolving
    @see cryptGetPassword, cryptCheckPassword
 */
PUBLIC char *cryptMakePassword(cchar *password, int saltLength, int rounds);

/**
    Check a plain-text password against a password hash.
    @description Verify a plain-text password against a previously computed Bcrypt hash.
        Uses constant-time comparison to prevent timing attacks.
    @param plainTextPassword Input plain-text password to verify. Must not be NULL.
    @param passwordHash Hash previously computed via cryptMakePassword. Must not be NULL.
    @return True if the password matches the hash, false otherwise.
    @stability Evolving
    @see cryptGetPassword, cryptMakePassword
 */
PUBLIC bool cryptCheckPassword(cchar *plainTextPassword, cchar *passwordHash);
#endif

/**
    Read a password from the console.
    @description Used by utility programs to read passwords from the console with echo disabled.
        Suitable for interactive password entry in command-line applications.
    @param prompt Password user prompt to display. Must not be NULL.
    @return The input password string. Caller must free.
    @stability Evolving
    @see cryptMakePassword
 */
PUBLIC char *cryptGetPassword(cchar *prompt);

/**
    Get random data.
    @description Fill a buffer with cryptographically secure random data from the system's random number generator.
    @param buf Result buffer to hold the random data. Must not be NULL.
    @param length Size of the buffer in bytes. Must be > 0.
    @param block Set to true to use blocking random generator that guarantees
        high-entropy random data even when system entropy is low.
    @return Zero on success, negative on error.
    @stability Evolving
    @see cryptID
 */
PUBLIC int cryptGetRandomBytes(uchar *buf, ssize length, bool block);

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
PUBLIC int cryptVerify(RKey *skey, uchar *sum, ssize sumsize, uchar *signature, ssize siglen);
#endif

/**
    Generate a random ID.
    @description Generate a random alphanumeric identifier string of specified length.
        Uses cryptographically secure random data for ID generation.
    @param size Size of the ID string to generate. Must be > 0.
    @return The random ID string. Caller must free.
    @stability Evolving
    @see cryptGetRandomBytes
 */
PUBLIC char *cryptID(ssize size);

/**
    Compare two strings in constant time.
    @description Compare two strings using constant-time comparison to prevent timing attacks.
        Both strings must be the same length. Use for comparing sensitive data like passwords or tokens.
    @param a First string to compare. Must not be NULL.
    @param b Second string to compare. Must not be NULL.
    @return True if the strings match, false otherwise.
    @stability Evolving
    @see cryptCheckPassword
 */
PUBLIC bool cryptMatch(cchar *a, cchar *b);

#ifdef __cplusplus
}
#endif

#endif /* _h_CRYPT */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
