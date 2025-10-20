/*
    helpers.c - AWS API helper routines that support SigV4 signed HTTP REST requests.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    Docs re Sigv4 signing
        https://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
        https://docs.aws.amazon.com/general/latest/gr/sigv4-signed-request-examples.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/CommonParameters.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/API_PutLogEvents.html
        https://github.com/fromkeith/awsgo/blob/master/cloudwatch/putLogEvents.go
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_CLOUD

#if ME_COM_MBEDTLS
    #include "mbedtls/mbedtls_config.h"
    #include "mbedtls/ssl.h"
    #include "mbedtls/ssl_cache.h"
    #include "mbedtls/ssl_ticket.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/net_sockets.h"
    #include "psa/crypto.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/error.h"
    #include "mbedtls/check_config.h"
#endif

#if ME_COM_OPENSSL
    #include <openssl/evp.h>
    #include <openssl/hmac.h>
#endif

/*********************************** Forwards *********************************/

static char *hashToString(char hash[CRYPT_SHA256_SIZE]);
static char *getHeader(cchar *headers, cchar *header, cchar *defaultValue);

/************************************* Code ***********************************/

static void sign(char hash[CRYPT_SHA256_SIZE], cchar *key, size_t keyLen, cchar *payload, size_t payloadLen)
{
    if (payloadLen == 0) {
        payloadLen = slen(payload);
    }
    if (keyLen == 0) {
        keyLen = slen(key);
    }
#if ME_COM_MBEDTLS
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t    md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (cuchar*) key, keyLen);
    mbedtls_md_hmac_update(&ctx, (cuchar*) payload, payloadLen);
    mbedtls_md_hmac_finish(&ctx, (uchar*) hash);
    mbedtls_md_free(&ctx);

#elif ME_COM_OPENSSL
    uint len;
    HMAC(EVP_sha256(), (uchar*) key, (int) keyLen, (uchar*) payload, (size_t) payloadLen, (uchar*) hash, &len);
#endif
}

static void genKey(char result[CRYPT_SHA256_SIZE], cchar *secret, cchar *date, cchar *region, cchar *service)
{
    char cacheKey[512], kDate[CRYPT_SHA256_SIZE], kRegion[CRYPT_SHA256_SIZE], kService[CRYPT_SHA256_SIZE];
    char *asecret;

    sfmtbuf(cacheKey, sizeof(cacheKey), "%s-%s-%s-%s", secret, date, region, service);
#if FUTURE
    if ((key = rLookupName(signHash, cacheKey)) != 0) {
        memcpy(result, key, CRYPT_SHA256_SIZE);
        return;
    }
#endif
    asecret = sfmt("AWS4%s", secret);
    sign(kDate, asecret, 0, date, 0);
    sign(kRegion, (cchar*) kDate, CRYPT_SHA256_SIZE, region, 0);
    sign(kService, (cchar*) kRegion, CRYPT_SHA256_SIZE, service, 0);
    sign(result, (cchar*) kService, CRYPT_SHA256_SIZE, "aws4_request", 0);
    rFree(asecret);

#if FUTURE
    key = rAlloc(CRYPT_SHA256_SIZE);
    memcpy(key, result, CRYPT_SHA256_SIZE);
    rAddName(signHash, cacheKey, key, R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
#endif
}

static void getHash(cchar *buf, size_t buflen, char hash[CRYPT_SHA256_SIZE])
{
    if (buflen == 0) {
        buflen = slen(buf);
    }
    cryptGetSha256Block((cuchar*) buf, buflen, (uchar*) hash);
}

static char *hashToString(char hash[CRYPT_SHA256_SIZE])
{
    return cryptSha256HashToString((uchar*) hash);
}

PUBLIC char *awsSign(cchar *region, cchar *service, cchar *target, cchar *method, cchar *path, cchar *query,
                     cchar *body, size_t bodyLen, cchar *headersFmt, ...)
{
    va_list args;
    Ticks   now;
    RBuf    *buf;
    cchar   *algorithm;
    char    *canonicalHeaders, *finalHeaders, *signedHeaders;
    char    *authorization, *canonicalRequest, *contentType, *date;
    char    *headers, *host, *isoDate, *scope, *time, *toSign;
    char    *hash, *payloadHash, *signature;
    char    shabuf[CRYPT_SHA256_SIZE], key[CRYPT_SHA256_SIZE];

    if (!service || !region) {
        rError("cloud.aws", "Missing service or region");
        return 0;
    }
    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return 0;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    if (body && bodyLen < 0) {
        bodyLen = slen(body);
    }
    if (!query) {
        query = "";
    }
    if (!target) {
        target = getHeader(headers, "x-amz-target", 0);
    }
    host = getHeader(headers, "Host", 0);
    if (!host) {
        host = sfmt("%s.%s.amazonaws.com", service, region);
    }
    if (smatch(service, "s3")) {
        target = 0;
        contentType = getHeader(headers, "content-type", "application/octet-stream");
    } else {
        contentType = getHeader(headers, "content-type", "application/x-amz-json-1.1");
    }

    /*
        Get date and time in required formats
     */
    now = rGetTime();
    time = rFormatUniversalTime("%Y%m%dT%H%M%SZ", now);
    date = rFormatUniversalTime("%Y%m%d", now);
    isoDate = rFormatUniversalTime("%a, %d %b %Y %T GMT", now);

    /*
        Get body hash
     */
    memset(shabuf, 0, sizeof(shabuf));
    if (body) {
        getHash(body, bodyLen, shabuf);
    } else {
        getHash("", 0, shabuf);
    }
    payloadHash = hashToString(shabuf);

    /*
        Create a canonical request to sign. Does not include all headers. Lower case, no spaces, sorted.
        Headers must be lowercase, no spaces, and be in alphabetic order
     */
    buf = rAllocBuf(0);
    rPutToBuf(buf,
              "content-type:%s\n"
              "host:%s\n",
              contentType, host);
    rPutToBuf(buf, "x-amz-date:%s\n", time);

    //  S3 requires x-amz-content-sha256 - hash of payload -- must be before x-amz-date
    if (ioto->awsToken) {
        rPutToBuf(buf, "x-amz-security-token:%s\n", ioto->awsToken);
    }
    if (target) {
        rPutToBuf(buf, "x-amz-target:%s\n", target);
    }
    canonicalHeaders = sclone(rBufToString(buf));

    /*
        Create headers to sign
     */
    rFlushBuf(buf);

    //  S3 sha must be before amz-date
    rPutToBuf(buf, "content-type;host;x-amz-date");
    if (ioto->awsToken) {
        rPutStringToBuf(buf, ";x-amz-security-token");
    }
    if (target) {
        rPutStringToBuf(buf, ";x-amz-target");
    }
    signedHeaders = sclone(rBufToString(buf));

    canonicalRequest =
        sfmt("%s\n/%s\n%s\n%s\n%s\n%s", method, path, query, canonicalHeaders, signedHeaders, payloadHash);
    rDebug("aws", "Canonical Headers\n%s\n", canonicalHeaders);
    rDebug("aws", "Canonical Request\n%s\n\n", canonicalRequest);

    getHash(canonicalRequest, 0, shabuf);
    hash = hashToString(shabuf);

    algorithm = "AWS4-HMAC-SHA256";
    scope = sfmt("%s/%s/%s/aws4_request", date, region, service);

    toSign = sfmt("%s\n%s\n%s\n%s", algorithm, time, scope, hash);
    rDebug("aws", "ToSign\n%s\n", toSign);

    genKey(key, ioto->awsSecret, date, region, service);

    sign(shabuf, key, CRYPT_SHA256_SIZE, toSign, 0);
    signature = hashToString(shabuf);

    authorization = sfmt("%s Credential=%s/%s, SignedHeaders=%s, Signature=%s",
                         algorithm, ioto->awsAccess, scope, signedHeaders, signature);
    rFlushBuf(buf);
    rPutToBuf(buf,
              "Authorization: %s\r\n"
              "Date: %s\r\n"
              "X-Amz-Content-sha256: %s\r\n"
              "X-Amz-Date: %s\r\n",
              authorization, isoDate, payloadHash, time);

    if (ioto->awsToken) {
        rPutToBuf(buf, "X-Amz-Security-Token: %s\r\n", ioto->awsToken);
    }
    if (target && *target) {
        rPutToBuf(buf, "X-Amz-Target: %s\r\n", target);
    }
    if (!getHeader(headers, "content-type", 0)) {
        rPutToBuf(buf, "Content-Type: %s\r\n", contentType);
    }
    if (headers) {
        rPutStringToBuf(buf, headers);
    }
    finalHeaders = sclone(rBufToString(buf));

    rFree(authorization);
    rFreeBuf(buf);
    rFree(canonicalHeaders);
    rFree(canonicalRequest);
    rFree(contentType);
    rFree(date);
    rFree(hash);
    rFree(host);
    rFree(isoDate);
    rFree(payloadHash);
    rFree(scope);
    rFree(signature);
    rFree(signedHeaders);
    rFree(time);
    rFree(toSign);
    rFree(headers);

    return finalHeaders;
}

/*
    Convenience routine to issue an AWS API request
 */
PUBLIC int aws(Url *up, cchar *region, cchar *service, cchar *target, cchar *body, size_t bodyLen, cchar *headersFmt,
               ...)
{
    va_list args;
    char    *headers, *signedHeaders, *url;
    int     status;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    signedHeaders = awsSign(region, service, target, "POST", "", NULL, body, bodyLen, headers);

    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.aws", "60 secs")) * TPS);
    url = sfmt("https://%s.%s.amazonaws.com/", service, region);
    status = urlFetch(up, "POST", url, body, bodyLen, signedHeaders);

    if (status != 200) {
        rError("aws", "AWS request failed: %s, status: %d, error: %s", url, status, urlGetResponse(up));
    }
    rFree(url);
    rFree(signedHeaders);
    rFree(headers);
    return status;
}

/*
    Simple request header extraction. Must be no spaces after ":"
    Caller must free result.
 */
static char *getHeader(cchar *headers, cchar *header, cchar *defaultValue)
{
    cchar *end, *start, *value;

    if (headers && header) {
        if ((start = sncaselesscontains(headers, header, 0)) != 0) {
            if ((value = schr(start, ':')) != 0) {
                while (*value++ == ' ') value++;
                if ((end = schr(value, '\r')) != 0) {
                    return snclone(value, (size_t) (end - value));
                }
            }
        }
    }
    if (defaultValue) {
        return sclone(defaultValue);
    }
    return 0;
}

/*
    https://docs.aws.amazon.com/AmazonS3/latest/API/API_PutObject.html
    https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-authenticating-requests.html

    If key is null, the basename of the file is used
 */
PUBLIC int awsPutFileToS3(cchar *region, cchar *bucket, cchar *key, cchar *file)
{
    char   *data;
    size_t length;
    int    rc;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    if (!key) {
        key = rBasename(file);
    }
    data = rReadFile(file, &length);
    rc = awsPutToS3(region, bucket, key, data, length);
    rFree(data);
    return rc;
}

PUBLIC int awsPutToS3(cchar *region, cchar *bucket, cchar *key, cchar *data, size_t dataLen)
{
    Url   *up;
    cchar *error;
    char  *path, *headers, *host, *url;
    int   status;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    if (data && dataLen == 0) {
        dataLen = slen(data);
    }
    up = urlAlloc(0);

    if (schr(bucket, '.')) {
        /*
            Path style is deprecated (almost) by AWS but preserved because virtual style does not (yet) work with
            buckets containing dots.
         */
        host = sfmt("s3.%s.amazonaws.com", region);
        path = sfmt("%s/%s", bucket, key);
        headers = awsSign(region, "s3", NULL, "PUT", path, NULL, data, dataLen, "Host:%s\r\n", host);
        url = sfmt("https://%s/%s", host, path);
        rFree(path);

    } else {
        /*
            Virtual style s3 host request
            Ref: https://docs.aws.amazon.com/AmazonS3/latest/userguide/RESTAPI.html
         */
        host = sfmt("%s.s3.%s.amazonaws.com", bucket, region);
        headers = awsSign(region, "s3", NULL, "PUT", key, NULL, data, dataLen, "Host:%s\r\n", host);
        url = sfmt("https://%s/%s", host, key);
    }
    status = urlFetch(up, "PUT", url, data, dataLen, headers);

    if (status != URL_CODE_OK) {
        error = up->error ? up->error : urlGetResponse(up);
        rError("cloud", "Cannot put to S3 %s%s. %s", host, key, error);
    }
    rFree(headers);
    rFree(url);
    rFree(host);
    return status == URL_CODE_OK ? 0 : R_ERR_CANT_WRITE;
}

#else
void dummyHelpers(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
