/*
    default-me.h -- Default Configure Header
 */

/* Settings */
#ifndef ME_VERSION
    #define ME_VERSION "2.4.0"
#endif
#ifndef ME_APP
    #define ME_APP "ioto"
#endif
#ifndef ME_AUTHOR
    #define ME_AUTHOR "Embedthis Software."
#endif
#ifndef ME_COMPANY
    #define ME_COMPANY "embedthis"
#endif
#ifndef ME_COMPILER_HAS_ATOMIC
    #if defined(_WIN32)
        #define ME_COMPILER_HAS_ATOMIC 0
    #else
        #define ME_COMPILER_HAS_ATOMIC 1
    #endif
#endif
#ifndef ME_COMPILER_HAS_ATOMIC64
    #define ME_COMPILER_HAS_ATOMIC64 1
#endif
#ifndef ME_COMPILER_HAS_DOUBLE_BRACES
    #define ME_COMPILER_HAS_DOUBLE_BRACES 1
#endif
#ifndef ME_COMPILER_HAS_DYN_LOAD
    #define ME_COMPILER_HAS_DYN_LOAD 1
#endif
#ifndef ME_COMPILER_HAS_LIB_EDIT
    #define ME_COMPILER_HAS_LIB_EDIT 0
#endif
#ifndef ME_COMPILER_HAS_LIB_RT
    #define ME_COMPILER_HAS_LIB_RT 1
#endif
#ifndef ME_COMPILER_HAS_MMU
    #define ME_COMPILER_HAS_MMU 1
#endif
#ifndef ME_COMPILER_HAS_MTUNE
    #define ME_COMPILER_HAS_MTUNE 1
#endif
#ifndef ME_COMPILER_HAS_PAM
    #define ME_COMPILER_HAS_PAM 0
#endif
#ifndef ME_COMPILER_HAS_STACK_PROTECTOR
    #define ME_COMPILER_HAS_STACK_PROTECTOR 1
#endif
#ifndef ME_COMPILER_HAS_SYNC
    #define ME_COMPILER_HAS_SYNC 1
#endif
#ifndef ME_COMPILER_HAS_SYNC64
    #define ME_COMPILER_HAS_SYNC64 1
#endif
#ifndef ME_COMPILER_HAS_SYNC_CAS
    #define ME_COMPILER_HAS_SYNC_CAS 1
#endif
#ifndef ME_COMPILER_HAS_UNNAMED_UNIONS
    #define ME_COMPILER_HAS_UNNAMED_UNIONS 1
#endif
#ifndef ME_COMPILER_WARN64TO32
    #define ME_COMPILER_WARN64TO32 0
#endif
#ifndef ME_COMPILER_WARN_UNUSED
    #define ME_COMPILER_WARN_UNUSED 1
#endif
#ifndef ME_DEBUG
    #define ME_DEBUG 1
#endif
#ifndef ME_DEPTH
    #define ME_DEPTH 1
#endif
#ifndef ME_DESCRIPTION
    #define ME_DESCRIPTION "Ioto Device agent"
#endif
#ifndef ME_GROUP
    #define ME_GROUP "ioto"
#endif
#ifndef ME_NAME
    #define ME_NAME "ioto"
#endif
#ifndef ME_STATIC
    #define ME_STATIC 1
#endif
#ifndef ME_TITLE
    #define ME_TITLE "Ioto"
#endif
#ifndef ME_TUNE
    #define ME_TUNE "size"
#endif
#ifndef ME_USER
    #define ME_USER "ioto"
#endif
#ifndef ME_WEB_AUTH
    #define ME_WEB_AUTH 1
#endif
#ifndef ME_WEB_LIMITS
    #define ME_WEB_LIMITS 1
#endif
#ifndef ME_WEB_SESSIONS
    #define ME_WEB_SESSIONS 1
#endif
#ifndef ME_WEB_UPLOAD
    #define ME_WEB_UPLOAD 1
#endif
#ifndef ME_WEB_GROUP
    #define ME_WEB_GROUP "nogroup"
#endif
#ifndef ME_WEB_USER
    #define ME_WEB_USER "nobody"
#endif

/* Components */
#ifndef ME_COM_OPENAI
    #define ME_COM_OPENAI 1
#endif
#ifndef ME_COM_CC
    #define ME_COM_CC 1
#endif
#ifndef ME_COM_CRYPT
    #define ME_COM_CRYPT 1
#endif
#ifndef ME_COM_DB
    #define ME_COM_DB 1
#endif
#ifndef ME_COM_JSON
    #define ME_COM_JSON 1
#endif
#ifndef ME_COM_MBEDTLS
    #define ME_COM_MBEDTLS 0
#endif
#ifndef ME_COM_MQTT
    #define ME_COM_MQTT 1
#endif
#ifndef ME_COM_OPENAI
    #define ME_COM_OPENAI 1
#endif
#ifndef ME_COM_OPENSSL
    #define ME_COM_OPENSSL 1
#endif
#ifndef ME_COM_OSDEP
    #define ME_COM_OSDEP 1
#endif
#ifndef ME_COM_R
    #define ME_COM_R 1
#endif
#ifndef ME_COM_SSL
    #define ME_COM_SSL 1
#endif
#ifndef ME_COM_UCTX
    #define ME_COM_UCTX 1
#endif
#ifndef ME_COM_URL
    #define ME_COM_URL 1
#endif
#ifndef ME_COM_WEB
    #define ME_COM_WEB 1
#endif
#ifndef ME_COM_WEBSOCK
    #define ME_COM_WEBSOCK 1
#endif
