/*
    config.h -- Ioto Configuration Header

    Default service and build configuration. All settings use #ifndef guards
    so they can be overridden via compiler -D flags (e.g. -DME_COM_MQTT=0).

    Platform-specific defaults use compiler built-in defines (__APPLE__,
    _WIN32, __linux__, VXWORKS, FREERTOS) for auto-detection.
 */

#ifndef _h_CONFIG
#define _h_CONFIG              1

/*
    Agent Services
 */
#ifndef SERVICES_AI
    #define SERVICES_AI        1
#endif
#ifndef SERVICES_CLOUD
    #define SERVICES_CLOUD     0
#endif
#ifndef SERVICES_DATABASE
    #define SERVICES_DATABASE  1
#endif
#ifndef SERVICES_MQTT
    #define SERVICES_MQTT      0
#endif
#ifndef SERVICES_UPDATE
    #define SERVICES_UPDATE    0
#endif
#ifndef SERVICES_URL
    #define SERVICES_URL       1
#endif
#ifndef SERVICES_WEB
    #define SERVICES_WEB       1
#endif

/*
    Settings
 */
#ifndef ME_AUTHOR
    #define ME_AUTHOR      "Embedthis Software"
#endif
#ifndef ME_COMPANY
    #define ME_COMPANY     "embedthis"
#endif
#ifndef ME_DESCRIPTION
    #define ME_DESCRIPTION "Ioto Device agent"
#endif
#ifndef ME_NAME
    #define ME_NAME        "ioto"
#endif
#ifndef ME_TITLE
    #define ME_TITLE       "Ioto"
#endif

/*
    Compiler Capabilities
 */
#ifndef ME_COMPILER_HAS_ATOMIC
    #if defined(_WIN32)
        #define ME_COMPILER_HAS_ATOMIC 0
    #else
        #define ME_COMPILER_HAS_ATOMIC 1
    #endif
#endif
#ifndef ME_COMPILER_HAS_DOUBLE_BRACES
    #if defined(VXWORKS)
        #define ME_COMPILER_HAS_DOUBLE_BRACES 0
    #else
        #define ME_COMPILER_HAS_DOUBLE_BRACES 1
    #endif
#endif
#ifndef ME_COMPILER_HAS_MMU
    #define ME_COMPILER_HAS_MMU               1
#endif
#ifndef ME_COMPILER_HAS_SYNC
    #if defined(_WIN32) || defined(FREERTOS) || defined(VXWORKS)
        #define ME_COMPILER_HAS_SYNC          0
    #else
        #define ME_COMPILER_HAS_SYNC          1
    #endif
#endif

/*
    Web Server
 */
#ifndef ME_WEB_AUTH
    #define ME_WEB_AUTH      1
#endif
#ifndef ME_WEB_LIMITS
    #define ME_WEB_LIMITS    1
#endif
#ifndef ME_WEB_SESSIONS
    #define ME_WEB_SESSIONS  1
#endif
#ifndef ME_WEB_UPLOAD
    #define ME_WEB_UPLOAD    1
#endif
#ifndef ME_WEB_GROUP
    #if defined(__APPLE__)
        #define ME_WEB_GROUP "_www"
    #elif defined(_WIN32)
        #define ME_WEB_GROUP "Administrator"
    #elif defined(FREERTOS) || defined(VXWORKS)
        #define ME_WEB_GROUP ""
    #else
        #define ME_WEB_GROUP "nogroup"
    #endif
#endif
#ifndef ME_WEB_USER
    #if defined(__APPLE__)
        #define ME_WEB_USER "_www"
    #elif defined(_WIN32)
        #define ME_WEB_USER "Administrator"
    #elif defined(FREERTOS) || defined(VXWORKS)
        #define ME_WEB_USER ""
    #else
        #define ME_WEB_USER "nobody"
    #endif
#endif

/*
    Components
 */
#ifndef ME_COM_CC
    #define ME_COM_CC      1
#endif
#ifndef ME_COM_CRYPT
    #define ME_COM_CRYPT   1
#endif
#ifndef ME_COM_DB
    #define ME_COM_DB      1
#endif
#ifndef ME_COM_JSON
    #define ME_COM_JSON    1
#endif
#ifndef ME_COM_LIB
    #define ME_COM_LIB     1
#endif
#ifndef ME_COM_MBEDTLS
    #define ME_COM_MBEDTLS 0
#endif
#ifndef ME_COM_MQTT
    #define ME_COM_MQTT    1
#endif
#ifndef ME_COM_OPENAI
    #define ME_COM_OPENAI  1
#endif
#ifndef ME_COM_OPENSSL
    #define ME_COM_OPENSSL 1
#endif
#ifndef ME_COM_OSDEP
    #define ME_COM_OSDEP   1
#endif
#ifndef ME_COM_R
    #define ME_COM_R       1
#endif
#ifndef ME_COM_SSL
    #define ME_COM_SSL     1
#endif
#ifndef ME_COM_UCTX
    #define ME_COM_UCTX    1
#endif
#ifndef ME_COM_URL
    #define ME_COM_URL     1
#endif
#ifndef ME_COM_WEB
    #define ME_COM_WEB     1
#endif
#ifndef ME_COM_WEBSOCK
    #define ME_COM_WEBSOCK 1
#endif

#endif /* _h_CONFIG */
