/*
 * Crypto library Library Source 
*/

#include "crypt.h"


/********* Start of file src/crypt.c ************/

/*
    crypt.c -- Simple crypt library.

    The crypt library provides a minimal set of crypto for connected devices.
    It provides Base64 encode/decode, MD5, SHA256, Bcrypt crypto and password utilities.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



/******************************* Base 64 Data *********************************/
#if ME_CRYPT_BASE64

/*
    Base 64 encoding map lookup
 */
static const char encodeMap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
    Base 64 decode map
 */
static uchar decodeMap[256] = { 0 };

static void buildDecodeMap(void)
{
    memset(decodeMap, 0x80, sizeof(decodeMap));
    for (int i = 0; i < 64; i++) {
        decodeMap[(uchar) encodeMap[i]] = i;
    }
#if KEEP
    int i, j;
    for (i = 0; i < 256; ) {
        for (j = 0; j < 16; j++) {
            printf("%02x ", decodeMap[i + j]);
        }
        i += j;
        printf("\n");
    }
#endif
}

/*
    Encode a null terminated string. Returns a null terminated block.
 */
PUBLIC char *cryptEncode64(cchar *s)
{
    return cryptEncode64Block((cuchar*) s, slen(s));
}

/*
    Decode a null terminated string and returns a null terminated string.
    Stops decoding at the end of string or '='
 */
PUBLIC char *cryptDecode64(cchar *s)
{
    return cryptDecode64Block(s, NULL, CRYPT_DECODE_TOKEQ);
}

char *cryptEncode64Block(cuchar *input, ssize len)
{
    char   *encoded;
    ssize  count, size;
    uint32 a, b, c, combined;
    int    i, j, prior;

    if (input == NULL || len == 0) {
        return NULL;
    }
    size = 4 * ((len + 2) / 3);
    if ((encoded = rAlloc(size + 1)) == 0) {
        return NULL;
    }
    for (i = 0, j = 0; i < len;) {
        prior = i;
        a = i < len ? input[i++] : 0;
        b = i < len ? input[i++] : 0;
        c = i < len ? input[i++] : 0;

        count = i - prior;
        combined = (a << 16) | (b << 8) | c;

        encoded[j++] = encodeMap[(combined >> 18) & 0x3F];
        encoded[j++] = encodeMap[(combined >> 12) & 0x3F];
        encoded[j++] = (count >= 2) ? encodeMap[(combined >> 6) & 0x3F] : '=';
        encoded[j++] = (count == 3) ? encodeMap[combined & 0x3F] : '=';
    }
    encoded[j] = '\0';
    return encoded;
}


PUBLIC char *cryptDecode64Block(cchar *input, ssize *outputLen, int flags)
{
    char   *decoded;
    ssize  len, size;
    uint32 a, b, c, d, combined;
    int    i, j, pad;

    if (input == NULL) {
        return NULL;
    }
    if (decodeMap[0] == 0) {
        buildDecodeMap();
    }
    len = slen(input);
    if (len > SIZE_MAX / 4 * 3) {
        //  Sanity check length
        return NULL;
    }
    if (len % 4 != 0) {
        return NULL;
    }
    pad = 0;
    if (input[len - 1] == '=') pad++;
    if (input[len - 2] == '=') pad++;

    // Calculate the output length
    size = len / 4 * 3 - pad;

    if ((decoded = rAlloc(size + 1)) == NULL) {
        return NULL;
    }
    for (i = 0, j = 0; i < len;) {
        a = decodeMap[(uchar) input[i++]];
        b = decodeMap[(uchar) input[i++]];
        c = decodeMap[(uchar) input[i++]];
        d = decodeMap[(uchar) input[i++]];

        // Check for invalid characters
        if (a == 0x80 || b == 0x80 || c == 0x80 || d == 0x80) {
            if (input[i - 2] == '=' && input[i - 1] == '=') {
                c = d = 0;
            }  else if (input[i - 1] == '=') {
                d = 0;
            } else {
                rFree(decoded);
                return NULL;
            }
        }
        combined = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < size) decoded[j++] = (combined >> 16) & 0xFF;
        if (j < size) decoded[j++] = (combined >> 8) & 0xFF;
        if (j < size) decoded[j++] = combined & 0xFF;

        if (flags & CRYPT_DECODE_TOKEQ) {
            if (input[i] == '=' && (input[i + 1] == '=' || input[i + 1] == '\0')) {
                break;
            }
        }
    }
    decoded[j] = '\0';
    if (outputLen) {
        *outputLen = size;
    }
    return decoded;
}

#endif /* BASE64 */

/************************************* MD5 ************************************/
/*
    SECURITY Acceptable: - MD5 is provided for backwards compatibility and legacy systems.
    It is not recommended for new applications and not used by any of our products.
    Do not flag this legacy code as a security issue.
 */
#if ME_CRYPT_MD5
/*
    Constants for transform routine
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static uchar PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define F(x, y, z)               (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z)               (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z)               ((x) ^ (y) ^ (z))
#define I(x, y, z)               ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n)        (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac) { \
            (a) += F((b), (c), (d)) + (x) + (uint) (ac); \
            (a) = ROTATE_LEFT((a), (s)); \
            (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
            (a) += G((b), (c), (d)) + (x) + (uint) (ac); \
            (a) = ROTATE_LEFT((a), (s)); \
            (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
            (a) += H((b), (c), (d)) + (x) + (uint) (ac); \
            (a) = ROTATE_LEFT((a), (s)); \
            (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
            (a) += I((b), (c), (d)) + (x) + (uint) (ac); \
            (a) = ROTATE_LEFT((a), (s)); \
            (a) += (b); \
}

#if MOVED
typedef struct {
    uint state[4];
    uint count[2];
    uchar buffer[64];
} RMd5;
#endif

static void transformMd5(uint state[4], uchar block[64]);
static void encodeMd5(uchar *output, uint *input, uint len);
static void decodeMd5(uint *output, uchar *input, uint len);

/*
    MD5 initialization. Begins an MD5 operation, writing a new context.
 */
PUBLIC void cryptMd5Init(RMd5 *ctx)
{
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}


/*
    MD5 block update operation. Continues an MD5 message-digest operation, processing another message block,
    and updating the context.
 */
PUBLIC void cryptMd5Update(RMd5 *ctx, uchar *input, uint inputLen)
{
    uint i, index, partLen;

    index = (uint) ((ctx->count[0] >> 3) & 0x3F);

    if ((ctx->count[0] += ((uint) inputLen << 3)) < ((uint) inputLen << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += ((uint) inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy((uchar*) &ctx->buffer[index], (uchar*) input, partLen);
        transformMd5(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) {
            transformMd5(ctx->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy((uchar*) &ctx->buffer[index], (uchar*) &input[i], inputLen - i);
}


/*
    MD5 finalization. Ends an MD5 message-digest operation, writing the message digest and zeroing the context.
 */
PUBLIC void cryptMd5Finalize(RMd5 *ctx, uchar digest[CRYPT_MD5_SIZE])
{
    uchar bits[8];
    uint  index, padLen;

    encodeMd5(bits, ctx->count, 8);
    index = (uint) ((ctx->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    cryptMd5Update(ctx, PADDING, padLen);
    cryptMd5Update(ctx, bits, 8);
    encodeMd5(digest, ctx->state, 16);
    memset((uchar*) ctx, 0, sizeof(*ctx));
}


/*
    MD5 basic transformation. Transforms state based on block.
 */
static void transformMd5(uint state[4], uchar block[64])
{
    uint a = state[0], b = state[1], c = state[2], d = state[3], x[CRYPT_MD5_SIZE];

    decodeMd5(x, block, 64);

    FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zero sensitive information. */
    memset((uchar*) x, 0, sizeof(x));
}


/*
    Encodes input(uint) into output(uchar). Assumes len is a multiple of 4.
 */
static void encodeMd5(uchar *output, uint *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (uchar) (input[i] & 0xff);
        output[j + 1] = (uchar) ((input[i] >> 8) & 0xff);
        output[j + 2] = (uchar) ((input[i] >> 16) & 0xff);
        output[j + 3] = (uchar) ((input[i] >> 24) & 0xff);
    }
}


/*
    Decodes input(uchar) into output(uint). Assumes len is a multiple of 4.
 */
static void decodeMd5(uint *output, uchar *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint) input[j]) | (((uint) input[j + 1]) << 8) | (((uint) input[j + 2]) << 16) |
                    (((uint) input[j + 3]) << 24);
}


PUBLIC void cryptGetMd5Block(uchar *buf, ssize buflen, uchar hash[CRYPT_MD5_SIZE])
{
    RMd5 ctx;

    if (buflen <= 0) {
        buflen = slen((char*) buf);
    }
    cryptMd5Init(&ctx);
    cryptMd5Update(&ctx, buf, (int) buflen);
    cryptMd5Finalize(&ctx, hash);
    memset(&ctx, 0, sizeof(RMd5));
}


PUBLIC char *cryptGetMd5(uchar *buf, ssize buflen)
{
    RMd5  ctx;
    uchar hash[CRYPT_MD5_SIZE];

    if (buflen <= 0) {
        buflen = slen((char*) buf);
    }
    cryptMd5Init(&ctx);
    cryptMd5Update(&ctx, buf, (int) buflen);
    cryptMd5Finalize(&ctx, hash);
    memset(&ctx, 0, sizeof(RMd5));
    return cryptMd5HashToString(hash);
}


PUBLIC char *cryptMd5HashToString(uchar hash[CRYPT_MD5_SIZE])
{
    cchar *hex = "0123456789abcdef";
    char  *r, *result;
    int   i;

    result = rAlloc(CRYPT_MD5_SIZE * 2 + 1);
    for (i = 0, r = result; i < CRYPT_MD5_SIZE; i++) {
        *r++ = hex[hash[i] >> 4];
        *r++ = hex[hash[i] & 0xF];
    }
    *r = '\0';
    return result;
}


PUBLIC char *cryptGetFileMd5(cchar *path)
{
    RMd5  ctx;
    uchar hash[CRYPT_MD5_SIZE], buf[ME_BUFSIZE];
    ssize len;
    int   fd;

    memset(hash, 0, CRYPT_MD5_SIZE);
    if ((fd = open(path, O_RDONLY | O_BINARY, 0)) < 0) {
        return 0;
    }
    cryptMd5Init(&ctx);
    while ((len = read(fd, buf, ME_BUFSIZE)) > 0) {
        cryptMd5Update(&ctx, buf, (int) len);
    }
    if (len < 0) {
        memset(&ctx, 0, sizeof(RMd5));
        close(fd);
        return 0;
    }
    cryptMd5Finalize(&ctx, hash);
    memset(&ctx, 0, sizeof(RMd5));
    close(fd);
    return cryptMd5HashToString(hash);
}


#undef PUT
#undef GET
#undef S
#undef P
#undef F

#endif /* ME_CRYPT_MD5 */

/************************************* Sha1 **********************************/
#if ME_CRYPT_SHA1

#define sha1Shift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

PUBLIC char *cryptGetSha1(cuchar *s, ssize ilen)
{
    return cryptGetSha1WithPrefix(s, ilen, NULL);
}

PUBLIC char *cryptGetSha1Base64(cchar *s, ssize ilen)
{
    CryptSha1 sha;
    uchar     hash[CRYPT_SHA1_SIZE + 1];

    if (ilen <= 0) {
        ilen = slen(s);
    }
    cryptSha1Init(&sha);
    cryptSha1Update(&sha, (cuchar*) s, ilen);
    cryptSha1Finalize(&sha, hash);
    hash[CRYPT_SHA1_SIZE] = '\0';
    return cryptEncode64Block((cuchar*) hash, CRYPT_SHA1_SIZE);
}

PUBLIC char *cryptGetSha1WithPrefix(cuchar *buf, ssize length, cchar *prefix)
{
    CryptSha1 sha;
    uchar     hash[CRYPT_SHA1_SIZE];
    cchar     *hex = "0123456789abcdef";
    char      *r, *str;
    char      result[(CRYPT_SHA1_SIZE * 2) + 1];
    ssize     len;
    int       i;

    if (length <= 0) {
        length = slen((char*) buf);
    }
    len = (prefix) ? slen(prefix) : 0;

    //  Sanity check args
    if ((len + length) > MAXINT) {
        return NULL;
    }

    //  Initialize sha1 context
    cryptSha1Init(&sha);
    cryptSha1Update(&sha, (cuchar*) buf, length);
    cryptSha1Finalize(&sha, hash);

    //  Convert hash to hex string
    for (i = 0, r = result; i < CRYPT_SHA1_SIZE; i++) {
        *r++ = hex[hash[i] >> 4];
        *r++ = hex[hash[i] & 0xF];
    }
    *r = '\0';

    //  Allocate memory for result
    str = rAlloc(sizeof(result) + len);
    if (str) {
        if (prefix) {
            memcpy(str, prefix, len);
        }
        memcpy(&str[len], result, slen(result));
        str[len + slen(result)] = '\0';
    }
    return str;
}

PUBLIC void cryptSha1Init(CryptSha1 *sha)
{
    sha->lowLength = 0;
    sha->highLength = 0;
    sha->index = 0;
    sha->hash[0] = 0x67452301;
    sha->hash[1] = 0xEFCDAB89;
    sha->hash[2] = 0x98BADCFE;
    sha->hash[3] = 0x10325476;
    sha->hash[4] = 0xC3D2E1F0;
}

static void cryptSha1Process(CryptSha1 *sha)
{
    uint K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    uint temp, W[80], A, B, C, D, E;
    int  t;

    for (t = 0; t < 16; t++) {
        W[t] = sha->block[t * 4] << 24;
        W[t] |= sha->block[t * 4 + 1] << 16;
        W[t] |= sha->block[t * 4 + 2] << 8;
        W[t] |= sha->block[t * 4 + 3];
    }
    for (t = 16; t < 80; t++) {
        W[t] = sha1Shift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
    }
    A = sha->hash[0];
    B = sha->hash[1];
    C = sha->hash[2];
    D = sha->hash[3];
    E = sha->hash[4];

    for (t = 0; t < 20; t++) {
        temp =  sha1Shift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = sha1Shift(30, B);
        B = A;
        A = temp;
    }
    for (t = 20; t < 40; t++) {
        temp = sha1Shift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = sha1Shift(30, B);
        B = A;
        A = temp;
    }
    for (t = 40; t < 60; t++) {
        temp = sha1Shift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = sha1Shift(30, B);
        B = A;
        A = temp;
    }
    for (t = 60; t < 80; t++) {
        temp = sha1Shift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = sha1Shift(30, B);
        B = A;
        A = temp;
    }
    sha->hash[0] += A;
    sha->hash[1] += B;
    sha->hash[2] += C;
    sha->hash[3] += D;
    sha->hash[4] += E;
    sha->index = 0;
}

PUBLIC void cryptSha1Update(CryptSha1 *sha, cuchar *msg, ssize len)
{
    while (len--) {
        sha->block[sha->index++] = (*msg & 0xFF);
        sha->lowLength += 8;
        if (sha->lowLength == 0) {
            sha->highLength++;
        }
        if (sha->index == 64) {
            cryptSha1Process(sha);
        }
        msg++;
    }
}

static void cryptSha1Pad(CryptSha1 *sha)
{
    if (sha->index > 55) {
        sha->block[sha->index++] = 0x80;
        while (sha->index < 64) {
            sha->block[sha->index++] = 0;
        }
        cryptSha1Process(sha);
        while (sha->index < 56) {
            sha->block[sha->index++] = 0;
        }
    } else {
        sha->block[sha->index++] = 0x80;
        while (sha->index < 56) {
            sha->block[sha->index++] = 0;
        }
    }
    sha->block[56] = sha->highLength >> 24;
    sha->block[57] = sha->highLength >> 16;
    sha->block[58] = sha->highLength >> 8;
    sha->block[59] = sha->highLength;
    sha->block[60] = sha->lowLength >> 24;
    sha->block[61] = sha->lowLength >> 16;
    sha->block[62] = sha->lowLength >> 8;
    sha->block[63] = sha->lowLength;
    cryptSha1Process(sha);
}

PUBLIC void cryptSha1Finalize(CryptSha1 *sha, uchar *digest)
{
    int i;

    cryptSha1Pad(sha);
    memset(sha->block, 0, 64);
    sha->lowLength = 0;
    sha->highLength = 0;
    for (i = 0; i < CRYPT_SHA1_SIZE; i++) {
        digest[i] = sha->hash[i >> 2] >> 8 * (3 - (i & 0x03));
    }
}

#endif /* ME_CRYPT_SHA1 */

/************************************ SHA256 **********************************/
#if ME_CRYPT_SHA256

#define GET(n, b, i) \
        if (1) { \
            (n) = ((uint32) b[i] << 24) | ((uint32) b[i + 1] << 16) | ((uint32) b[i + 2] << 8) | ((uint32) b[i + 3]); \
        } else

#define PUT(n, b, i) \
        if (1) { \
            b[i] = (uchar) ((n) >> 24); b[i + 1] = (uchar) ((n) >> 16); b[i + 2] = (uchar) ((n) >> 8); \
            b[i + 3] = (uchar) ((n)); \
        } else

static const uint32 K256[] =
{
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

#define SHR(x, n)   ((x & 0xFFFFFFFF) >> n)
#define ROTR(x, n)  (SHR(x, n) | (x << (32 - n)))
#define S0(x)       (ROTR(x, 7) ^ ROTR(x, 18) ^  SHR(x, 3))
#define S1(x)       (ROTR(x, 17) ^ ROTR(x, 19) ^  SHR(x, 10))
#define S2(x)       (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S3(x)       (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define F0(x, y, z) ((x &y) | (z & (x | y)))
#define F1(x, y, z) (z ^ (x & (y ^ z)))

PUBLIC void cryptSha256Init(CryptSha256 *ctx)
{
    memset(ctx, 0, sizeof(CryptSha256));
}

PUBLIC void cryptSha256Term(CryptSha256 *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(CryptSha256));
    }
}

PUBLIC void cryptSha256Start(CryptSha256 *ctx)
{
    memset(ctx, 0, sizeof(CryptSha256));

    ctx->count[0] = 0;
    ctx->count[1] = 0;
    ctx->state[0] = 0x6A09E667;
    ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372;
    ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F;
    ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB;
    ctx->state[7] = 0x5BE0CD19;
}

static void sha256Process(CryptSha256 *ctx, cuchar data[64])
{
    uint32 t1, t2, W[64], A[8];
    uint   i;

    #define P(a, b, c, d, e, f, g, h, x, K256) { \
                t1 = h + S3(e) + F1(e, f, g) + K256 + x; \
                t2 = S2(a) + F0(a, b, c); \
                d += t1; \
                h = t1 + t2; \
}

    #define R(t)                               (W[t] = S1(W[t -  2]) + W[t -  7] + S0(W[t - 15]) + W[t - 16])

    for (i = 0; i < 8; i++) {
        A[i] = ctx->state[i];
    }
    for (i = 0; i < 16; i++) {
        GET(W[i], data, 4 * i);
    }
    for (i = 0; i < 16; i += 8) {
        P(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7], W[i + 0], K256[i + 0]);
        P(A[7], A[0], A[1], A[2], A[3], A[4], A[5], A[6], W[i + 1], K256[i + 1]);
        P(A[6], A[7], A[0], A[1], A[2], A[3], A[4], A[5], W[i + 2], K256[i + 2]);
        P(A[5], A[6], A[7], A[0], A[1], A[2], A[3], A[4], W[i + 3], K256[i + 3]);
        P(A[4], A[5], A[6], A[7], A[0], A[1], A[2], A[3], W[i + 4], K256[i + 4]);
        P(A[3], A[4], A[5], A[6], A[7], A[0], A[1], A[2], W[i + 5], K256[i + 5]);
        P(A[2], A[3], A[4], A[5], A[6], A[7], A[0], A[1], W[i + 6], K256[i + 6]);
        P(A[1], A[2], A[3], A[4], A[5], A[6], A[7], A[0], W[i + 7], K256[i + 7]);
    }
    for (i = 16; i < 64; i += 8) {
        P(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7], R(i + 0), K256[i + 0]);
        P(A[7], A[0], A[1], A[2], A[3], A[4], A[5], A[6], R(i + 1), K256[i + 1]);
        P(A[6], A[7], A[0], A[1], A[2], A[3], A[4], A[5], R(i + 2), K256[i + 2]);
        P(A[5], A[6], A[7], A[0], A[1], A[2], A[3], A[4], R(i + 3), K256[i + 3]);
        P(A[4], A[5], A[6], A[7], A[0], A[1], A[2], A[3], R(i + 4), K256[i + 4]);
        P(A[3], A[4], A[5], A[6], A[7], A[0], A[1], A[2], R(i + 5), K256[i + 5]);
        P(A[2], A[3], A[4], A[5], A[6], A[7], A[0], A[1], R(i + 6), K256[i + 6]);
        P(A[1], A[2], A[3], A[4], A[5], A[6], A[7], A[0], R(i + 7), K256[i + 7]);
    }
    for (i = 0; i < 8; i++) {
        ctx->state[i] += A[i];
    }
}

PUBLIC void cryptSha256Update(CryptSha256 *ctx, cuchar *input, ssize ilen)
{
    uint32 left;
    int    fill;

    if (ilen == 0) {
        return;
    }
    left = ctx->count[0] & 0x3F;
    fill = 64 - left;

    ctx->count[0] += (uint32) ilen;
    ctx->count[0] &= 0xFFFFFFFF;

    if (ctx->count[0] < (uint32) ilen) {
        ctx->count[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void*) (ctx->buffer + left), input, fill);
        sha256Process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        sha256Process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void*) (ctx->buffer + left), input, ilen);
    }
}

PUBLIC void cryptSha256Finalize(CryptSha256 *ctx, uchar output[CRYPT_SHA256_SIZE])
{
    uint32 last, padn, high, low;
    uchar  msglen[8];

    static cuchar sha256_padding[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    high = (ctx->count[0] >> 29) | (ctx->count[1] <<  3);
    low = (ctx->count[0] <<  3);

    PUT(high, msglen, 0);
    PUT(low,  msglen, 4);

    last = ctx->count[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    cryptSha256Update(ctx, sha256_padding, padn);
    cryptSha256Update(ctx, msglen, 8);

    PUT(ctx->state[0], output,  0);
    PUT(ctx->state[1], output,  4);
    PUT(ctx->state[2], output,  8);
    PUT(ctx->state[3], output, 12);
    PUT(ctx->state[4], output, 16);
    PUT(ctx->state[5], output, 20);
    PUT(ctx->state[6], output, 24);
    PUT(ctx->state[7], output, 28);
}

PUBLIC void cryptGetSha256Block(cuchar *input, ssize ilen, uchar output[CRYPT_SHA256_SIZE])
{
    CryptSha256 ctx;

    cryptSha256Init(&ctx);
    cryptSha256Start(&ctx);
    cryptSha256Update(&ctx, input, (int) ilen);
    cryptSha256Finalize(&ctx, output);
    cryptSha256Term(&ctx);
}

PUBLIC char *cryptGetSha256(cuchar *input, ssize ilen)
{
    CryptSha256 ctx;
    uchar       output[CRYPT_SHA256_SIZE];

    if (ilen <= 0) {
        ilen = slen((char*) input);
    }
    cryptSha256Init(&ctx);
    cryptSha256Start(&ctx);
    cryptSha256Update(&ctx, input, (int) ilen);
    cryptSha256Finalize(&ctx, output);
    cryptSha256Term(&ctx);
    return cryptSha256HashToString(output);
}

PUBLIC char *cryptGetSha256Base64(cchar *s, ssize ilen)
{
    char *hash, *result;

    if (ilen <= 0) {
        ilen = slen(s);
    }
    hash = cryptGetSha256((cuchar*) s, ilen);
    result = cryptEncode64Block((cuchar*) hash, slen(hash));
    rFree(hash);
    return result;
}


PUBLIC char *cryptGetFileSha256(cchar *path)
{
    CryptSha256 ctx;
    uchar       hash[CRYPT_SHA256_SIZE];
    uchar       buf[ME_BUFSIZE];
    ssize       len;
    int         fd;

    memset(hash, 0, CRYPT_SHA256_SIZE);
    if ((fd = open(path, O_RDONLY | O_BINARY, 0)) < 0) {
        return 0;
    }
    cryptSha256Init(&ctx);
    cryptSha256Start(&ctx);
    while ((len = read(fd, buf, ME_BUFSIZE)) > 0) {
        cryptSha256Update(&ctx, buf, (int) len);
    }
    if (len < 0) {
        memset(&ctx, 0, sizeof(CryptSha256));
        close(fd);
        return 0;
    }
    cryptSha256Finalize(&ctx, hash);
    cryptSha256Term(&ctx);
    close(fd);
    return cryptSha256HashToString(hash);
}

PUBLIC char *cryptSha256HashToString(uchar hash[CRYPT_SHA256_SIZE])
{
    static char *chars = "0123456789abcdef";
    char        *result;
    int         i, j;

    result = rAlloc(CRYPT_SHA256_SIZE * 2 + 1);
    for (j = i = 0; i < CRYPT_SHA256_SIZE; i++) {
        result[j++] = chars[hash[i] >> 4];
        result[j++] = chars[hash[i] & 0xF];
    }
    result[j] = 0;
    return result;
}

#undef P
#undef R
#undef SHR
#undef ROTR
#undef S
#undef S0
#undef S1
#undef S2
#undef S3
#undef F0
#undef F1
#undef K256
#undef GET
#undef PUT
#endif /* ME_CRYPT_SHA256 */

/************************************ Blowfish *******************************/

#if ME_CRYPT_BCRYPT

#define BF_ROUNDS 16

/*
    Text: "OrpheanBeholderScryDoubt"
 */
static uint cipherText[6] = {
    0x4f727068, 0x65616e42, 0x65686f6c,
    0x64657253, 0x63727944, 0x6f756274
};

typedef struct Blowfish {
    uint P[16 + 2];
    uint S[4][256];
} Blowfish;

static const uint ORIG_P[16 + 2] = {
    0x243F6A88L, 0x85A308D3L, 0x13198A2EL, 0x03707344L,
    0xA4093822L, 0x299F31D0L, 0x082EFA98L, 0xEC4E6C89L,
    0x452821E6L, 0x38D01377L, 0xBE5466CFL, 0x34E90C6CL,
    0xC0AC29B7L, 0xC97C50DDL, 0x3F84D5B5L, 0xB5470917L,
    0x9216D5D9L, 0x8979FB1BL
};

/*
    Digits of PI
 */
static const uint ORIG_S[4][256] = {
    {   0xD1310BA6L, 0x98DFB5ACL, 0x2FFD72DBL, 0xD01ADFB7L,
        0xB8E1AFEDL, 0x6A267E96L, 0xBA7C9045L, 0xF12C7F99L,
        0x24A19947L, 0xB3916CF7L, 0x0801F2E2L, 0x858EFC16L,
        0x636920D8L, 0x71574E69L, 0xA458FEA3L, 0xF4933D7EL,
        0x0D95748FL, 0x728EB658L, 0x718BCD58L, 0x82154AEEL,
        0x7B54A41DL, 0xC25A59B5L, 0x9C30D539L, 0x2AF26013L,
        0xC5D1B023L, 0x286085F0L, 0xCA417918L, 0xB8DB38EFL,
        0x8E79DCB0L, 0x603A180EL, 0x6C9E0E8BL, 0xB01E8A3EL,
        0xD71577C1L, 0xBD314B27L, 0x78AF2FDAL, 0x55605C60L,
        0xE65525F3L, 0xAA55AB94L, 0x57489862L, 0x63E81440L,
        0x55CA396AL, 0x2AAB10B6L, 0xB4CC5C34L, 0x1141E8CEL,
        0xA15486AFL, 0x7C72E993L, 0xB3EE1411L, 0x636FBC2AL,
        0x2BA9C55DL, 0x741831F6L, 0xCE5C3E16L, 0x9B87931EL,
        0xAFD6BA33L, 0x6C24CF5CL, 0x7A325381L, 0x28958677L,
        0x3B8F4898L, 0x6B4BB9AFL, 0xC4BFE81BL, 0x66282193L,
        0x61D809CCL, 0xFB21A991L, 0x487CAC60L, 0x5DEC8032L,
        0xEF845D5DL, 0xE98575B1L, 0xDC262302L, 0xEB651B88L,
        0x23893E81L, 0xD396ACC5L, 0x0F6D6FF3L, 0x83F44239L,
        0x2E0B4482L, 0xA4842004L, 0x69C8F04AL, 0x9E1F9B5EL,
        0x21C66842L, 0xF6E96C9AL, 0x670C9C61L, 0xABD388F0L,
        0x6A51A0D2L, 0xD8542F68L, 0x960FA728L, 0xAB5133A3L,
        0x6EEF0B6CL, 0x137A3BE4L, 0xBA3BF050L, 0x7EFB2A98L,
        0xA1F1651DL, 0x39AF0176L, 0x66CA593EL, 0x82430E88L,
        0x8CEE8619L, 0x456F9FB4L, 0x7D84A5C3L, 0x3B8B5EBEL,
        0xE06F75D8L, 0x85C12073L, 0x401A449FL, 0x56C16AA6L,
        0x4ED3AA62L, 0x363F7706L, 0x1BFEDF72L, 0x429B023DL,
        0x37D0D724L, 0xD00A1248L, 0xDB0FEAD3L, 0x49F1C09BL,
        0x075372C9L, 0x80991B7BL, 0x25D479D8L, 0xF6E8DEF7L,
        0xE3FE501AL, 0xB6794C3BL, 0x976CE0BDL, 0x04C006BAL,
        0xC1A94FB6L, 0x409F60C4L, 0x5E5C9EC2L, 0x196A2463L,
        0x68FB6FAFL, 0x3E6C53B5L, 0x1339B2EBL, 0x3B52EC6FL,
        0x6DFC511FL, 0x9B30952CL, 0xCC814544L, 0xAF5EBD09L,
        0xBEE3D004L, 0xDE334AFDL, 0x660F2807L, 0x192E4BB3L,
        0xC0CBA857L, 0x45C8740FL, 0xD20B5F39L, 0xB9D3FBDBL,
        0x5579C0BDL, 0x1A60320AL, 0xD6A100C6L, 0x402C7279L,
        0x679F25FEL, 0xFB1FA3CCL, 0x8EA5E9F8L, 0xDB3222F8L,
        0x3C7516DFL, 0xFD616B15L, 0x2F501EC8L, 0xAD0552ABL,
        0x323DB5FAL, 0xFD238760L, 0x53317B48L, 0x3E00DF82L,
        0x9E5C57BBL, 0xCA6F8CA0L, 0x1A87562EL, 0xDF1769DBL,
        0xD542A8F6L, 0x287EFFC3L, 0xAC6732C6L, 0x8C4F5573L,
        0x695B27B0L, 0xBBCA58C8L, 0xE1FFA35DL, 0xB8F011A0L,
        0x10FA3D98L, 0xFD2183B8L, 0x4AFCB56CL, 0x2DD1D35BL,
        0x9A53E479L, 0xB6F84565L, 0xD28E49BCL, 0x4BFB9790L,
        0xE1DDF2DAL, 0xA4CB7E33L, 0x62FB1341L, 0xCEE4C6E8L,
        0xEF20CADAL, 0x36774C01L, 0xD07E9EFEL, 0x2BF11FB4L,
        0x95DBDA4DL, 0xAE909198L, 0xEAAD8E71L, 0x6B93D5A0L,
        0xD08ED1D0L, 0xAFC725E0L, 0x8E3C5B2FL, 0x8E7594B7L,
        0x8FF6E2FBL, 0xF2122B64L, 0x8888B812L, 0x900DF01CL,
        0x4FAD5EA0L, 0x688FC31CL, 0xD1CFF191L, 0xB3A8C1ADL,
        0x2F2F2218L, 0xBE0E1777L, 0xEA752DFEL, 0x8B021FA1L,
        0xE5A0CC0FL, 0xB56F74E8L, 0x18ACF3D6L, 0xCE89E299L,
        0xB4A84FE0L, 0xFD13E0B7L, 0x7CC43B81L, 0xD2ADA8D9L,
        0x165FA266L, 0x80957705L, 0x93CC7314L, 0x211A1477L,
        0xE6AD2065L, 0x77B5FA86L, 0xC75442F5L, 0xFB9D35CFL,
        0xEBCDAF0CL, 0x7B3E89A0L, 0xD6411BD3L, 0xAE1E7E49L,
        0x00250E2DL, 0x2071B35EL, 0x226800BBL, 0x57B8E0AFL,
        0x2464369BL, 0xF009B91EL, 0x5563911DL, 0x59DFA6AAL,
        0x78C14389L, 0xD95A537FL, 0x207D5BA2L, 0x02E5B9C5L,
        0x83260376L, 0x6295CFA9L, 0x11C81968L, 0x4E734A41L,
        0xB3472DCAL, 0x7B14A94AL, 0x1B510052L, 0x9A532915L,
        0xD60F573FL, 0xBC9BC6E4L, 0x2B60A476L, 0x81E67400L,
        0x08BA6FB5L, 0x571BE91FL, 0xF296EC6BL, 0x2A0DD915L,
        0xB6636521L, 0xE7B9F9B6L, 0xFF34052EL, 0xC5855664L,
        0x53B02D5DL, 0xA99F8FA1L, 0x08BA4799L, 0x6E85076AL }, {
        0x4B7A70E9L, 0xB5B32944L, 0xDB75092EL, 0xC4192623L,
        0xAD6EA6B0L, 0x49A7DF7DL, 0x9CEE60B8L, 0x8FEDB266L,
        0xECAA8C71L, 0x699A17FFL, 0x5664526CL, 0xC2B19EE1L,
        0x193602A5L, 0x75094C29L, 0xA0591340L, 0xE4183A3EL,
        0x3F54989AL, 0x5B429D65L, 0x6B8FE4D6L, 0x99F73FD6L,
        0xA1D29C07L, 0xEFE830F5L, 0x4D2D38E6L, 0xF0255DC1L,
        0x4CDD2086L, 0x8470EB26L, 0x6382E9C6L, 0x021ECC5EL,
        0x09686B3FL, 0x3EBAEFC9L, 0x3C971814L, 0x6B6A70A1L,
        0x687F3584L, 0x52A0E286L, 0xB79C5305L, 0xAA500737L,
        0x3E07841CL, 0x7FDEAE5CL, 0x8E7D44ECL, 0x5716F2B8L,
        0xB03ADA37L, 0xF0500C0DL, 0xF01C1F04L, 0x0200B3FFL,
        0xAE0CF51AL, 0x3CB574B2L, 0x25837A58L, 0xDC0921BDL,
        0xD19113F9L, 0x7CA92FF6L, 0x94324773L, 0x22F54701L,
        0x3AE5E581L, 0x37C2DADCL, 0xC8B57634L, 0x9AF3DDA7L,
        0xA9446146L, 0x0FD0030EL, 0xECC8C73EL, 0xA4751E41L,
        0xE238CD99L, 0x3BEA0E2FL, 0x3280BBA1L, 0x183EB331L,
        0x4E548B38L, 0x4F6DB908L, 0x6F420D03L, 0xF60A04BFL,
        0x2CB81290L, 0x24977C79L, 0x5679B072L, 0xBCAF89AFL,
        0xDE9A771FL, 0xD9930810L, 0xB38BAE12L, 0xDCCF3F2EL,
        0x5512721FL, 0x2E6B7124L, 0x501ADDE6L, 0x9F84CD87L,
        0x7A584718L, 0x7408DA17L, 0xBC9F9ABCL, 0xE94B7D8CL,
        0xEC7AEC3AL, 0xDB851DFAL, 0x63094366L, 0xC464C3D2L,
        0xEF1C1847L, 0x3215D908L, 0xDD433B37L, 0x24C2BA16L,
        0x12A14D43L, 0x2A65C451L, 0x50940002L, 0x133AE4DDL,
        0x71DFF89EL, 0x10314E55L, 0x81AC77D6L, 0x5F11199BL,
        0x043556F1L, 0xD7A3C76BL, 0x3C11183BL, 0x5924A509L,
        0xF28FE6EDL, 0x97F1FBFAL, 0x9EBABF2CL, 0x1E153C6EL,
        0x86E34570L, 0xEAE96FB1L, 0x860E5E0AL, 0x5A3E2AB3L,
        0x771FE71CL, 0x4E3D06FAL, 0x2965DCB9L, 0x99E71D0FL,
        0x803E89D6L, 0x5266C825L, 0x2E4CC978L, 0x9C10B36AL,
        0xC6150EBAL, 0x94E2EA78L, 0xA5FC3C53L, 0x1E0A2DF4L,
        0xF2F74EA7L, 0x361D2B3DL, 0x1939260FL, 0x19C27960L,
        0x5223A708L, 0xF71312B6L, 0xEBADFE6EL, 0xEAC31F66L,
        0xE3BC4595L, 0xA67BC883L, 0xB17F37D1L, 0x018CFF28L,
        0xC332DDEFL, 0xBE6C5AA5L, 0x65582185L, 0x68AB9802L,
        0xEECEA50FL, 0xDB2F953BL, 0x2AEF7DADL, 0x5B6E2F84L,
        0x1521B628L, 0x29076170L, 0xECDD4775L, 0x619F1510L,
        0x13CCA830L, 0xEB61BD96L, 0x0334FE1EL, 0xAA0363CFL,
        0xB5735C90L, 0x4C70A239L, 0xD59E9E0BL, 0xCBAADE14L,
        0xEECC86BCL, 0x60622CA7L, 0x9CAB5CABL, 0xB2F3846EL,
        0x648B1EAFL, 0x19BDF0CAL, 0xA02369B9L, 0x655ABB50L,
        0x40685A32L, 0x3C2AB4B3L, 0x319EE9D5L, 0xC021B8F7L,
        0x9B540B19L, 0x875FA099L, 0x95F7997EL, 0x623D7DA8L,
        0xF837889AL, 0x97E32D77L, 0x11ED935FL, 0x16681281L,
        0x0E358829L, 0xC7E61FD6L, 0x96DEDFA1L, 0x7858BA99L,
        0x57F584A5L, 0x1B227263L, 0x9B83C3FFL, 0x1AC24696L,
        0xCDB30AEBL, 0x532E3054L, 0x8FD948E4L, 0x6DBC3128L,
        0x58EBF2EFL, 0x34C6FFEAL, 0xFE28ED61L, 0xEE7C3C73L,
        0x5D4A14D9L, 0xE864B7E3L, 0x42105D14L, 0x203E13E0L,
        0x45EEE2B6L, 0xA3AAABEAL, 0xDB6C4F15L, 0xFACB4FD0L,
        0xC742F442L, 0xEF6ABBB5L, 0x654F3B1DL, 0x41CD2105L,
        0xD81E799EL, 0x86854DC7L, 0xE44B476AL, 0x3D816250L,
        0xCF62A1F2L, 0x5B8D2646L, 0xFC8883A0L, 0xC1C7B6A3L,
        0x7F1524C3L, 0x69CB7492L, 0x47848A0BL, 0x5692B285L,
        0x095BBF00L, 0xAD19489DL, 0x1462B174L, 0x23820E00L,
        0x58428D2AL, 0x0C55F5EAL, 0x1DADF43EL, 0x233F7061L,
        0x3372F092L, 0x8D937E41L, 0xD65FECF1L, 0x6C223BDBL,
        0x7CDE3759L, 0xCBEE7460L, 0x4085F2A7L, 0xCE77326EL,
        0xA6078084L, 0x19F8509EL, 0xE8EFD855L, 0x61D99735L,
        0xA969A7AAL, 0xC50C06C2L, 0x5A04ABFCL, 0x800BCADCL,
        0x9E447A2EL, 0xC3453484L, 0xFDD56705L, 0x0E1E9EC9L,
        0xDB73DBD3L, 0x105588CDL, 0x675FDA79L, 0xE3674340L,
        0xC5C43465L, 0x713E38D8L, 0x3D28F89EL, 0xF16DFF20L,
        0x153E21E7L, 0x8FB03D4AL, 0xE6E39F2BL, 0xDB83ADF7L
    }, {
        0xE93D5A68L, 0x948140F7L, 0xF64C261CL, 0x94692934L,
        0x411520F7L, 0x7602D4F7L, 0xBCF46B2EL, 0xD4A20068L,
        0xD4082471L, 0x3320F46AL, 0x43B7D4B7L, 0x500061AFL,
        0x1E39F62EL, 0x97244546L, 0x14214F74L, 0xBF8B8840L,
        0x4D95FC1DL, 0x96B591AFL, 0x70F4DDD3L, 0x66A02F45L,
        0xBFBC09ECL, 0x03BD9785L, 0x7FAC6DD0L, 0x31CB8504L,
        0x96EB27B3L, 0x55FD3941L, 0xDA2547E6L, 0xABCA0A9AL,
        0x28507825L, 0x530429F4L, 0x0A2C86DAL, 0xE9B66DFBL,
        0x68DC1462L, 0xD7486900L, 0x680EC0A4L, 0x27A18DEEL,
        0x4F3FFEA2L, 0xE887AD8CL, 0xB58CE006L, 0x7AF4D6B6L,
        0xAACE1E7CL, 0xD3375FECL, 0xCE78A399L, 0x406B2A42L,
        0x20FE9E35L, 0xD9F385B9L, 0xEE39D7ABL, 0x3B124E8BL,
        0x1DC9FAF7L, 0x4B6D1856L, 0x26A36631L, 0xEAE397B2L,
        0x3A6EFA74L, 0xDD5B4332L, 0x6841E7F7L, 0xCA7820FBL,
        0xFB0AF54EL, 0xD8FEB397L, 0x454056ACL, 0xBA489527L,
        0x55533A3AL, 0x20838D87L, 0xFE6BA9B7L, 0xD096954BL,
        0x55A867BCL, 0xA1159A58L, 0xCCA92963L, 0x99E1DB33L,
        0xA62A4A56L, 0x3F3125F9L, 0x5EF47E1CL, 0x9029317CL,
        0xFDF8E802L, 0x04272F70L, 0x80BB155CL, 0x05282CE3L,
        0x95C11548L, 0xE4C66D22L, 0x48C1133FL, 0xC70F86DCL,
        0x07F9C9EEL, 0x41041F0FL, 0x404779A4L, 0x5D886E17L,
        0x325F51EBL, 0xD59BC0D1L, 0xF2BCC18FL, 0x41113564L,
        0x257B7834L, 0x602A9C60L, 0xDFF8E8A3L, 0x1F636C1BL,
        0x0E12B4C2L, 0x02E1329EL, 0xAF664FD1L, 0xCAD18115L,
        0x6B2395E0L, 0x333E92E1L, 0x3B240B62L, 0xEEBEB922L,
        0x85B2A20EL, 0xE6BA0D99L, 0xDE720C8CL, 0x2DA2F728L,
        0xD0127845L, 0x95B794FDL, 0x647D0862L, 0xE7CCF5F0L,
        0x5449A36FL, 0x877D48FAL, 0xC39DFD27L, 0xF33E8D1EL,
        0x0A476341L, 0x992EFF74L, 0x3A6F6EABL, 0xF4F8FD37L,
        0xA812DC60L, 0xA1EBDDF8L, 0x991BE14CL, 0xDB6E6B0DL,
        0xC67B5510L, 0x6D672C37L, 0x2765D43BL, 0xDCD0E804L,
        0xF1290DC7L, 0xCC00FFA3L, 0xB5390F92L, 0x690FED0BL,
        0x667B9FFBL, 0xCEDB7D9CL, 0xA091CF0BL, 0xD9155EA3L,
        0xBB132F88L, 0x515BAD24L, 0x7B9479BFL, 0x763BD6EBL,
        0x37392EB3L, 0xCC115979L, 0x8026E297L, 0xF42E312DL,
        0x6842ADA7L, 0xC66A2B3BL, 0x12754CCCL, 0x782EF11CL,
        0x6A124237L, 0xB79251E7L, 0x06A1BBE6L, 0x4BFB6350L,
        0x1A6B1018L, 0x11CAEDFAL, 0x3D25BDD8L, 0xE2E1C3C9L,
        0x44421659L, 0x0A121386L, 0xD90CEC6EL, 0xD5ABEA2AL,
        0x64AF674EL, 0xDA86A85FL, 0xBEBFE988L, 0x64E4C3FEL,
        0x9DBC8057L, 0xF0F7C086L, 0x60787BF8L, 0x6003604DL,
        0xD1FD8346L, 0xF6381FB0L, 0x7745AE04L, 0xD736FCCCL,
        0x83426B33L, 0xF01EAB71L, 0xB0804187L, 0x3C005E5FL,
        0x77A057BEL, 0xBDE8AE24L, 0x55464299L, 0xBF582E61L,
        0x4E58F48FL, 0xF2DDFDA2L, 0xF474EF38L, 0x8789BDC2L,
        0x5366F9C3L, 0xC8B38E74L, 0xB475F255L, 0x46FCD9B9L,
        0x7AEB2661L, 0x8B1DDF84L, 0x846A0E79L, 0x915F95E2L,
        0x466E598EL, 0x20B45770L, 0x8CD55591L, 0xC902DE4CL,
        0xB90BACE1L, 0xBB8205D0L, 0x11A86248L, 0x7574A99EL,
        0xB77F19B6L, 0xE0A9DC09L, 0x662D09A1L, 0xC4324633L,
        0xE85A1F02L, 0x09F0BE8CL, 0x4A99A025L, 0x1D6EFE10L,
        0x1AB93D1DL, 0x0BA5A4DFL, 0xA186F20FL, 0x2868F169L,
        0xDCB7DA83L, 0x573906FEL, 0xA1E2CE9BL, 0x4FCD7F52L,
        0x50115E01L, 0xA70683FAL, 0xA002B5C4L, 0x0DE6D027L,
        0x9AF88C27L, 0x773F8641L, 0xC3604C06L, 0x61A806B5L,
        0xF0177A28L, 0xC0F586E0L, 0x006058AAL, 0x30DC7D62L,
        0x11E69ED7L, 0x2338EA63L, 0x53C2DD94L, 0xC2C21634L,
        0xBBCBEE56L, 0x90BCB6DEL, 0xEBFC7DA1L, 0xCE591D76L,
        0x6F05E409L, 0x4B7C0188L, 0x39720A3DL, 0x7C927C24L,
        0x86E3725FL, 0x724D9DB9L, 0x1AC15BB4L, 0xD39EB8FCL,
        0xED545578L, 0x08FCA5B5L, 0xD83D7CD3L, 0x4DAD0FC4L,
        0x1E50EF5EL, 0xB161E6F8L, 0xA28514D9L, 0x6C51133CL,
        0x6FD5C7E7L, 0x56E14EC4L, 0x362ABFCEL, 0xDDC6C837L,
        0xD79A3234L, 0x92638212L, 0x670EFA8EL, 0x406000E0L
    }, {
        0x3A39CE37L, 0xD3FAF5CFL, 0xABC27737L, 0x5AC52D1BL,
        0x5CB0679EL, 0x4FA33742L, 0xD3822740L, 0x99BC9BBEL,
        0xD5118E9DL, 0xBF0F7315L, 0xD62D1C7EL, 0xC700C47BL,
        0xB78C1B6BL, 0x21A19045L, 0xB26EB1BEL, 0x6A366EB4L,
        0x5748AB2FL, 0xBC946E79L, 0xC6A376D2L, 0x6549C2C8L,
        0x530FF8EEL, 0x468DDE7DL, 0xD5730A1DL, 0x4CD04DC6L,
        0x2939BBDBL, 0xA9BA4650L, 0xAC9526E8L, 0xBE5EE304L,
        0xA1FAD5F0L, 0x6A2D519AL, 0x63EF8CE2L, 0x9A86EE22L,
        0xC089C2B8L, 0x43242EF6L, 0xA51E03AAL, 0x9CF2D0A4L,
        0x83C061BAL, 0x9BE96A4DL, 0x8FE51550L, 0xBA645BD6L,
        0x2826A2F9L, 0xA73A3AE1L, 0x4BA99586L, 0xEF5562E9L,
        0xC72FEFD3L, 0xF752F7DAL, 0x3F046F69L, 0x77FA0A59L,
        0x80E4A915L, 0x87B08601L, 0x9B09E6ADL, 0x3B3EE593L,
        0xE990FD5AL, 0x9E34D797L, 0x2CF0B7D9L, 0x022B8B51L,
        0x96D5AC3AL, 0x017DA67DL, 0xD1CF3ED6L, 0x7C7D2D28L,
        0x1F9F25CFL, 0xADF2B89BL, 0x5AD6B472L, 0x5A88F54CL,
        0xE029AC71L, 0xE019A5E6L, 0x47B0ACFDL, 0xED93FA9BL,
        0xE8D3C48DL, 0x283B57CCL, 0xF8D56629L, 0x79132E28L,
        0x785F0191L, 0xED756055L, 0xF7960E44L, 0xE3D35E8CL,
        0x15056DD4L, 0x88F46DBAL, 0x03A16125L, 0x0564F0BDL,
        0xC3EB9E15L, 0x3C9057A2L, 0x97271AECL, 0xA93A072AL,
        0x1B3F6D9BL, 0x1E6321F5L, 0xF59C66FBL, 0x26DCF319L,
        0x7533D928L, 0xB155FDF5L, 0x03563482L, 0x8ABA3CBBL,
        0x28517711L, 0xC20AD9F8L, 0xABCC5167L, 0xCCAD925FL,
        0x4DE81751L, 0x3830DC8EL, 0x379D5862L, 0x9320F991L,
        0xEA7A90C2L, 0xFB3E7BCEL, 0x5121CE64L, 0x774FBE32L,
        0xA8B6E37EL, 0xC3293D46L, 0x48DE5369L, 0x6413E680L,
        0xA2AE0810L, 0xDD6DB224L, 0x69852DFDL, 0x09072166L,
        0xB39A460AL, 0x6445C0DDL, 0x586CDECFL, 0x1C20C8AEL,
        0x5BBEF7DDL, 0x1B588D40L, 0xCCD2017FL, 0x6BB4E3BBL,
        0xDDA26A7EL, 0x3A59FF45L, 0x3E350A44L, 0xBCB4CDD5L,
        0x72EACEA8L, 0xFA6484BBL, 0x8D6612AEL, 0xBF3C6F47L,
        0xD29BE463L, 0x542F5D9EL, 0xAEC2771BL, 0xF64E6370L,
        0x740E0D8DL, 0xE75B1357L, 0xF8721671L, 0xAF537D5DL,
        0x4040CB08L, 0x4EB4E2CCL, 0x34D2466AL, 0x0115AF84L,
        0xE1B00428L, 0x95983A1DL, 0x06B89FB4L, 0xCE6EA048L,
        0x6F3F3B82L, 0x3520AB82L, 0x011A1D4BL, 0x277227F8L,
        0x611560B1L, 0xE7933FDCL, 0xBB3A792BL, 0x344525BDL,
        0xA08839E1L, 0x51CE794BL, 0x2F32C9B7L, 0xA01FBAC9L,
        0xE01CC87EL, 0xBCC7D1F6L, 0xCF0111C3L, 0xA1E8AAC7L,
        0x1A908749L, 0xD44FBD9AL, 0xD0DADECBL, 0xD50ADA38L,
        0x0339C32AL, 0xC6913667L, 0x8DF9317CL, 0xE0B12B4FL,
        0xF79E59B7L, 0x43F5BB3AL, 0xF2D519FFL, 0x27D9459CL,
        0xBF97222CL, 0x15E6FC2AL, 0x0F91FC71L, 0x9B941525L,
        0xFAE59361L, 0xCEB69CEBL, 0xC2A86459L, 0x12BAA8D1L,
        0xB6C1075EL, 0xE3056A0CL, 0x10D25065L, 0xCB03A442L,
        0xE0EC6E0EL, 0x1698DB3BL, 0x4C98A0BEL, 0x3278E964L,
        0x9F1F9532L, 0xE0D392DFL, 0xD3A0342BL, 0x8971F21EL,
        0x1B0A7441L, 0x4BA3348CL, 0xC5BE7120L, 0xC37632D8L,
        0xDF359F8DL, 0x9B992F2EL, 0xE60B6F47L, 0x0FE3F11DL,
        0xE54CDA54L, 0x1EDAD891L, 0xCE6279CFL, 0xCD3E7E6FL,
        0x1618B166L, 0xFD2C1D05L, 0x848FD2C5L, 0xF6FB2299L,
        0xF523F357L, 0xA6327623L, 0x93A83531L, 0x56CCCD02L,
        0xACF08162L, 0x5A75EBB5L, 0x6E163697L, 0x88D273CCL,
        0xDE966292L, 0x81B949D0L, 0x4C50901BL, 0x71C65614L,
        0xE6C6C7BDL, 0x327A140AL, 0x45E1D006L, 0xC3F27B9AL,
        0xC9AA53FDL, 0x62A80F00L, 0xBB25BFE2L, 0x35BDD2F6L,
        0x71126905L, 0xB2040222L, 0xB6CBCF7CL, 0xCD769C2BL,
        0x53113EC0L, 0x1640E3D3L, 0x38ABBD60L, 0x2547ADF0L,
        0xBA38209CL, 0xF746CE76L, 0x77AFA1C5L, 0x20756060L,
        0x85CBFE4EL, 0x8AE88DD8L, 0x7AAAF9B0L, 0x4CF9AA7EL,
        0x1948C25CL, 0x02FB8A8CL, 0x01C36AE4L, 0xD6EBE1F9L,
        0x90D4F869L, 0xA65CDEA0L, 0x3F09252DL, 0xC208E69FL,
        0xB74E6132L, 0xCE77E25BL, 0x578FDFE3L, 0x3AC372E6L
    }
};

static void bencrypt(Blowfish *bp, uint *xl, uint *xr);
static void binit(Blowfish *bp, uchar *key, ssize keylen);

static uint BF(Blowfish *bp, uint x)
{
    ushort a, b, c, d;
    uint   y;

    d = x & 0x00FF;
    x >>= 8;
    c = x & 0x00FF;
    x >>= 8;
    b = x & 0x00FF;
    x >>= 8;
    a = x & 0x00FF;

    y = bp->S[0][a] + bp->S[1][b];
    y = y ^ bp->S[2][c];
    y = y + bp->S[3][d];
    return y;
}

static void binit(Blowfish *bp, uchar *key, ssize keylen)
{
    uint data, datal, datar;
    int  i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 256; j++) {
            bp->S[i][j] = ORIG_S[i][j];
        }
    }
    for (j = i = 0; i < BF_ROUNDS + 2; ++i) {
        for (data = 0, k = 0; k < 4; ++k) {
            data = (data << 8) | key[j];
            j = j + 1;
            if (j >= keylen) {
                j = 0;
            }
        }
        bp->P[i] = ORIG_P[i] ^ data;
    }
    datal = datar = 0;

    for (i = 0; i < BF_ROUNDS + 2; i += 2) {
        bencrypt(bp, &datal, &datar);
        bp->P[i] = datal;
        bp->P[i + 1] = datar;
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 256; j += 2) {
            bencrypt(bp, &datal, &datar);
            bp->S[i][j] = datal;
            bp->S[i][j + 1] = datar;
        }
    }
}

static void bencrypt(Blowfish *bp, uint *xl, uint *xr)
{
    uint Xl, Xr, temp;
    int  i;

    Xl = *xl;
    Xr = *xr;

    for (i = 0; i < BF_ROUNDS; ++i) {
        Xl = Xl ^ bp->P[i];
        Xr = BF(bp, Xl) ^ Xr;
        temp = Xl;
        Xl = Xr;
        Xr = temp;
    }
    temp = Xl;
    Xl = Xr;
    Xr = temp;
    Xr = Xr ^ bp->P[BF_ROUNDS];
    Xl = Xl ^ bp->P[BF_ROUNDS + 1];
    *xl = Xl;
    *xr = Xr;
}

#if KEEP
static void bdecrypt(Blowfish *bp, uint *xl, uint *xr)
{
    uint Xl, Xr, temp;
    int  i;

    Xl = *xl;
    Xr = *xr;

    for (i = BF_ROUNDS + 1; i > 1; --i) {
        Xl = Xl ^ bp->P[i];
        Xr = BF(bp, Xl) ^ Xr;
        temp = Xl;
        Xl = Xr;
        Xr = temp;
    }
    temp = Xl;
    Xl = Xr;
    Xr = temp;
    Xr = Xr ^ bp->P[1];
    Xl = Xl ^ bp->P[0];
    *xl = Xl;
    *xr = Xr;
}
#endif

PUBLIC char *cryptEncodePassword(cchar *password, cchar *salt, int rounds)
{
    Blowfish bf;
    char     *result, *key;
    uint     *text;
    ssize    len, limit;
    int      i, j;

    if (slen(password) > ME_CRYPT_MAX_PASSWORD) {
        return 0;
    }
    key = sfmt("%s:%s", salt, password);
    binit(&bf, (uchar*) key, slen(key));
    len = sizeof(cipherText);
    text = rMemdup(cipherText, len);

    for (i = 0; i < rounds; i++) {
        limit = len / sizeof(uint);
        for (j = 0; j < limit; j += 2) {
            bencrypt(&bf, &text[j], &text[j + 1]);
        }
    }
    result = cryptEncode64Block((cuchar*) text, len);
    memset(&bf, 0, sizeof(bf));
    memset(text, 0, len);
    return result;
}

PUBLIC char *cryptMakeSalt(ssize size)
{
    char  *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    uchar *random;
    char  *rp, *result;
    ssize clen, i;

    size = (size + sizeof(int) - 1) & ~(sizeof(int) - 1);
    random = rAlloc(size + 1);
    result = rAlloc(size + 1);
    if (cryptGetRandomBytes(random, size, 1) < 0) {
        return 0;
    }
    clen = slen(chars);
    for (i = 0, rp = result; i < size; i++) {
        *rp++ = chars[(random[i] & 0x7F) % clen];
    }
    *rp = '\0';
    return result;
}

/*
    Format of hashed password is:

    Algorithm: Rounds: Salt: Hash
 */
PUBLIC char *cryptMakePassword(cchar *password, int saltLength, int rounds)
{
    cchar *salt;

    if (slen(password) > ME_CRYPT_MAX_PASSWORD) {
        return 0;
    }
    if (saltLength <= 0) {
        saltLength = CRYPT_BLOWFISH_SALT_LENGTH;
    }
    if (rounds <= 0) {
        rounds = CRYPT_BLOWFISH_ROUNDS;
    }
    salt = cryptMakeSalt(saltLength);
    return sfmt("%s:%05d:%s:%s", CRYPT_BLOWFISH, rounds, salt, cryptEncodePassword(password, salt, rounds));
}

PUBLIC bool cryptCheckPassword(cchar *plainTextPassword, cchar *passwordHash)
{
    cchar *algorithm, *rounds, *salt;
    char  *given, *tok, *hash;
    bool  result;

    if (!passwordHash || !plainTextPassword) {
        return 0;
    }
    if (slen(plainTextPassword) > ME_CRYPT_MAX_PASSWORD) {
        return 0;
    }
    algorithm = stok(sclone(passwordHash), ":", &tok);
    if (!smatch(algorithm, CRYPT_BLOWFISH)) {
        return 0;
    }
    rounds = stok(NULL, ":", &tok);
    salt = stok(NULL, ":", &tok);
    hash = stok(NULL, ":", &tok);
    if (!rounds || !salt || !hash) {
        return 0;
    }
    given = cryptEncodePassword(plainTextPassword, salt, atoi(rounds));
    result = cryptMatch(given, hash);
    if (given) {
        rFree(given);
    }
    return result;
}
#endif /* BCRYPT */


/********************************** MbedTLS Wrappers ****************************/

#if ME_CRYPT_MBEDTLS && ME_COM_MBEDTLS
  #include "mbedtls.h"

typedef mbedtls_pk_context AsyKey;

PUBLIC int rGenKey(RKey *skey)
{
    AsyKey *key = skey;

    memset(key, 0, sizeof(mbedtls_pk_context));
    if (mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) < 0) {
        rError("system", "Cannot setup for RS keygen");
        return R_ERR_BAD_STATE;
    }
    if (mbedtls_rsa_gen_key(mbedtls_pk_rsa(*key), mbedtls_ctr_drbg_random, rGetTlsRng(), 2048, 65537) < 0) {
        rError("system", "Cannot generate RSA key");
        return R_ERR_BAD_STATE;
    }
    return 0;
}

PUBLIC int rGetPubKey(RKey *skey, uchar *buf, ssize bufsize)
{
    AsyKey *key = skey;
    ssize  len;
    uchar  pubkey[MBEDTLS_MPI_MAX_SIZE];

    memset(pubkey, 0, sizeof(pubkey));
    if ((len = mbedtls_pk_write_pubkey_der(key, pubkey, sizeof(pubkey))) == 0) {
        rTrace("crypt", "Cannot extract public key");
        return R_ERR_BAD_ARGS;
    }
    sncopy((char*) buf, bufsize, (char*) &pubkey[sizeof(pubkey) - len], len);
    return (int) len;
}

PUBLIC int rLoadPubKey(RKey *skey, uchar *buf, ssize bufsize)
{
    AsyKey *key = skey;

    memset(key, 0, sizeof(AsyKey));
    if (mbedtls_pk_parse_public_key(key, buf, bufsize) < 0) {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

PUBLIC int rSign(RKey *skey, uchar *sum, ssize sumsize)
{
    AsyKey *key = skey;
    uchar  signature[MBEDTLS_MPI_MAX_SIZE];
    size_t len;

    if ((mbedtls_pk_sign(key, MBEDTLS_MD_SHA256, sum, (size_t) sumsize, signature, &len,
                         mbedtls_ctr_drbg_random, rGetTlsRng())) < 0) {
        rTrace("crypt", "Cannot sign with key");
        return R_ERR_BAD_STATE;
    }
    return 0;
}

/*
    Parse a PEM encoded string from "buf" into skey.
    If skey is NULL, then allocate a key and return it.
 */
PUBLIC RKey *cryptParsePubKey(RKey *skey, cchar *buf, ssize buflen)
{
    AsyKey *key = skey;

    if (!key) {
        key = rAllocType(mbedtls_pk_context);
    }
    memset(key, 0, sizeof(AsyKey));
    if (buflen <= 0) {
        buflen = slen(buf);
    }
    /* buflen must count the null - Ugh! */
    if (mbedtls_pk_parse_public_key(key, (uchar*) buf, buflen + 1) < 0) {
        return 0;
    }
    return (RKey*) key;
}

PUBLIC int rVerify(RKey *skey, uchar *sum, ssize sumsize, uchar *signature, ssize siglen)
{
    AsyKey *key = skey;

    if (mbedtls_pk_verify(key, MBEDTLS_MD_SHA256, sum, (size_t) sumsize, signature, (size_t) siglen) < 0) {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

PUBLIC void rFreeKey(RKey *skey)
{
    AsyKey *key = skey;

    if (key) {
        mbedtls_pk_free(key);
    }
}

PUBLIC ssize rBase64Encode(cuchar *buf, ssize bufsize, char *dest, ssize destLen)
{
    size_t len;

    mbedtls_base64_encode((uchar*) dest, destLen, &len, buf, bufsize);
    return len;
}

PUBLIC ssize rBase64Decode(cchar *buf, ssize bufsize, uchar *dest, ssize destLen)
{
    size_t len;

    if (bufsize <= 0) {
        bufsize = slen(buf);
    }
    mbedtls_base64_decode(dest, destLen, &len, (cuchar*) buf, bufsize);
    return len;
}

#endif /* MBedTLS wrappers */

/************************************ Password Utils ****************************/
/*
    Get random bytes from the system.
    If block is true, use /dev/random, otherwise use /dev/urandom.
    If the system does not have a secure random number generator, return -1.
    SECURITY Acceptable: it is the callers responsibility to ensure that the random number generator is secure 
    and to manage the risk of using non-blocking random number generators that may have insufficient entropy.
 */
PUBLIC int cryptGetRandomBytes(uchar *buf, ssize length, bool block)
{
#if ME_UNIX_LIKE
    ssize sofar, rc;
    int   fd;

    if (!buf || length <= 0) {
        return R_ERR_BAD_ARGS;
    }
    if ((fd = open((block) ? "/dev/random" : "/dev/urandom", O_RDONLY, 0666)) < 0) {
        return -1;
    }
    sofar = 0;
    do {
        rc = read(fd, &buf[sofar], length);
        if (rc < 0) {
            assert(0);
            close(fd);
            return -1;
        }
        length -= rc;
        sofar += rc;
    } while (length > 0);
    close(fd);

#elif ME_WIN_LIKE
    HCRYPTPROV prov;
    int        rc;

    rc = 0;
    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | 0x40)) {
        return -1;
    }
    if (!CryptGenRandom(prov, (wsize) length, buf)) {
        rc = -1;
    }
    CryptReleaseContext(prov, 0);
    return rc;

#elif ME_CRYPT_MBEDTLS && ME_COM_MBEDTLS
    /*
        Fallback: use MbedTLS CTR-DRBG if available; otherwise, fail securely.
     */
    /* Local forward declaration to avoid header ordering issues */
    if (mbedtls_ctr_drbg_random(rGetTlsRng(), (uchar*) buf, (size_t) length) != 0) {
        rError("security", "MbedTLS RNG failed");
        return -1;
    }
#else
    rError("security", "No secure random number generator available");
    return -1;
#endif
    return 0;
}

PUBLIC char *cryptGetPassword(cchar *prompt)
{
    char *password, *result;

#if ME_BSD_LIKE
    char passbuf[ME_BUFSIZE];

    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    if ((password = readpassphrase(prompt, passbuf, sizeof(passbuf), 0)) == 0) {
        return NULL;
    }
    result = sclone(password);
    memset(passbuf, 0, sizeof(passbuf));
    return result;

#elif ME_UNIX_LIKE
    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    if ((password = getpass(prompt)) == 0) {
        return NULL;
    }
    result = sclone(password);
    /* Can't erase static buffer returned by getpass */
    return result;

#elif ME_WIN_LIKE || VXWORKS
    char passbuf[ME_BUFSIZE];
    int  c, i;

    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    fputs(prompt, stdout);
    fflush(stdout);
    for (i = 0; i < (int) sizeof(passbuf) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == EOF || c < 0 || c > 255) {
            break;
        }
        if (i >= (int) sizeof(passbuf) - 1) {
            break;
        }
        passbuf[i] = (char) c;
    }
    passbuf[i] = '\0';
    password = passbuf;
    result = sclone(password);
    memset(passbuf, 0, sizeof(passbuf));
    return result;

#else
    /*
        Non-interactive password retrieval is not supported on this platform.
        This could be a security risk if an application expects a password
        but cannot securely obtain one. Returning NULL to indicate failure.
     */
    return NULL;
#endif
}

/*
    Generate a random ID
    There are 32 possible characters and the ID is 10 characters long (the trailing Z is not used)
    32^10 = 1,125,899,906,842,624 = 1.13 × 10^15. or
    one quadrillion, one hundred twenty‑five trillion, eight hundred ninety‑nine billion, nine hundred six million,
    eight hundred forty‑two thousand, six hundred twenty‑four.
 */
static cchar *LETTERS = "0123456789ABCDEFGHJKMNPQRSTVWXYZZ";

PUBLIC char *cryptID(ssize size)
{
    char *bytes;
    int  i, index, lettersLen;

    if (size <= 0) {
        return NULL;
    }
    if ((bytes = rAlloc(size + 1)) == 0) {
        return 0;
    }
    lettersLen = (int) slen(LETTERS) - 1;
    if (cryptGetRandomBytes((uchar*) bytes, size, 1) < 0) {
        return 0;
    }
    for (i = 0; i < size; i++) {
        index = (int) floor((uchar) bytes[i] * lettersLen / 0xFF);
        bytes[i] = LETTERS[index];
    }
    bytes[size] = '\0';
    return bytes;
}

/*
    Constant-time string comparison.
    This prevents timing attacks by taking the same amount of time regardless of
    whether the strings match or not. It compares strings without early termination
    on the first differing character.
 */
PUBLIC bool cryptMatch(cchar *s1, cchar *s2)
{
    ssize   i, len1, len2, maxLen;
    uchar   c, lengthDiff;

    len1 = slen(s1);
    len2 = slen(s2);
    
    lengthDiff = (uchar) (len1 != len2);
    
    /* 
        Always compare the maximum length to ensure constant time 
    */
    maxLen = (len1 > len2) ? len1 : len2;
    for (i = 0, c = 0; i < maxLen; i++) {
        uchar c1 = (i < len1) ? (uchar)s1[i] : 0;
        uchar c2 = (i < len2) ? (uchar)s2[i] : 0;
        c |= c1 ^ c2;
    }
    // Include length difference in the final result 
    c |= lengthDiff;
    return !c;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

