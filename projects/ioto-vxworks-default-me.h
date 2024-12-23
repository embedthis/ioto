/*
    me.h -- MakeMe Configure Header for vxworks-arm-default
 */

/* Settings */
#ifndef ME_AUTHOR
    #define ME_AUTHOR "Embedthis Software."
#endif
#ifndef ME_COMPANY
    #define ME_COMPANY "embedthis"
#endif
#ifndef ME_COMPATIBLE
    #define ME_COMPATIBLE "2.5"
#endif
#ifndef ME_COMPILER_HAS_ATOMIC
    #define ME_COMPILER_HAS_ATOMIC 0
#endif
#ifndef ME_COMPILER_HAS_ATOMIC64
    #define ME_COMPILER_HAS_ATOMIC64 0
#endif
#ifndef ME_COMPILER_HAS_DOUBLE_BRACES
    #define ME_COMPILER_HAS_DOUBLE_BRACES 0
#endif
#ifndef ME_COMPILER_HAS_DYN_LOAD
    #define ME_COMPILER_HAS_DYN_LOAD 1
#endif
#ifndef ME_COMPILER_HAS_LIB_EDIT
    #define ME_COMPILER_HAS_LIB_EDIT 0
#endif
#ifndef ME_COMPILER_HAS_LIB_RT
    #define ME_COMPILER_HAS_LIB_RT 0
#endif
#ifndef ME_COMPILER_HAS_MMU
    #define ME_COMPILER_HAS_MMU 1
#endif
#ifndef ME_COMPILER_HAS_MTUNE
    #define ME_COMPILER_HAS_MTUNE 0
#endif
#ifndef ME_COMPILER_HAS_PAM
    #define ME_COMPILER_HAS_PAM 0
#endif
#ifndef ME_COMPILER_HAS_STACK_PROTECTOR
    #define ME_COMPILER_HAS_STACK_PROTECTOR 1
#endif
#ifndef ME_COMPILER_HAS_SYNC
    #define ME_COMPILER_HAS_SYNC 0
#endif
#ifndef ME_COMPILER_HAS_SYNC64
    #define ME_COMPILER_HAS_SYNC64 0
#endif
#ifndef ME_COMPILER_HAS_SYNC_CAS
    #define ME_COMPILER_HAS_SYNC_CAS 0
#endif
#ifndef ME_COMPILER_HAS_UNNAMED_UNIONS
    #define ME_COMPILER_HAS_UNNAMED_UNIONS 1
#endif
#ifndef ME_COMPILER_WARN64TO32
    #define ME_COMPILER_WARN64TO32 0
#endif
#ifndef ME_COMPILER_WARN_UNUSED
    #define ME_COMPILER_WARN_UNUSED 0
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
#ifndef ME_MANIFEST
    #define ME_MANIFEST "installs/manifest.me"
#endif
#ifndef ME_NAME
    #define ME_NAME "ioto"
#endif
#ifndef ME_PARTS
    #define ME_PARTS "undefined"
#endif
#ifndef ME_PLATFORMS
    #define ME_PLATFORMS "local"
#endif
#ifndef ME_PREFIXES
    #define ME_PREFIXES "install-prefixes"
#endif
#ifndef ME_STATIC
    #define ME_STATIC 1
#endif
#ifndef ME_TITLE
    #define ME_TITLE "Ioto"
#endif
#ifndef ME_TLS
    #define ME_TLS "openssl"
#endif
#ifndef ME_TUNE
    #define ME_TUNE "size"
#endif
#ifndef ME_USER
    #define ME_USER "ioto"
#endif
#ifndef ME_VERSION
    #define ME_VERSION "2.5.0"
#endif
#ifndef ME_WEB_GROUP
    #define ME_WEB_GROUP ""
#endif
#ifndef ME_WEB_USER
    #define ME_WEB_USER ""
#endif

/* Prefixes */
#ifndef ME_ROOT_PREFIX
    #define ME_ROOT_PREFIX "deploy"
#endif
#ifndef ME_BASE_PREFIX
    #define ME_BASE_PREFIX "deploy"
#endif
#ifndef ME_DATA_PREFIX
    #define ME_DATA_PREFIX "deploy"
#endif
#ifndef ME_STATE_PREFIX
    #define ME_STATE_PREFIX "deploy"
#endif
#ifndef ME_BIN_PREFIX
    #define ME_BIN_PREFIX "deploy"
#endif
#ifndef ME_INC_PREFIX
    #define ME_INC_PREFIX "deploy/inc"
#endif
#ifndef ME_LIB_PREFIX
    #define ME_LIB_PREFIX "deploy"
#endif
#ifndef ME_MAN_PREFIX
    #define ME_MAN_PREFIX "deploy"
#endif
#ifndef ME_SBIN_PREFIX
    #define ME_SBIN_PREFIX "deploy"
#endif
#ifndef ME_ETC_PREFIX
    #define ME_ETC_PREFIX "deploy"
#endif
#ifndef ME_WEB_PREFIX
    #define ME_WEB_PREFIX "deploy/web"
#endif
#ifndef ME_LOG_PREFIX
    #define ME_LOG_PREFIX "deploy"
#endif
#ifndef ME_SPOOL_PREFIX
    #define ME_SPOOL_PREFIX "deploy"
#endif
#ifndef ME_CACHE_PREFIX
    #define ME_CACHE_PREFIX "deploy"
#endif
#ifndef ME_APP_PREFIX
    #define ME_APP_PREFIX "deploy"
#endif
#ifndef ME_VAPP_PREFIX
    #define ME_VAPP_PREFIX "deploy"
#endif
#ifndef ME_SRC_PREFIX
    #define ME_SRC_PREFIX "/usr/src/ioto-2.5.0"
#endif

/* Suffixes */
#ifndef ME_EXE
    #define ME_EXE ".out"
#endif
#ifndef ME_SHLIB
    #define ME_SHLIB ".out"
#endif
#ifndef ME_SHOBJ
    #define ME_SHOBJ ".out"
#endif
#ifndef ME_LIB
    #define ME_LIB ".a"
#endif
#ifndef ME_OBJ
    #define ME_OBJ ".o"
#endif

/* Profile */
#ifndef ME_CONFIG_CMD
    #define ME_CONFIG_CMD "me -d -q -platform vxworks-arm-default -configure . -gen make"
#endif
#ifndef ME_IOTO_PRODUCT
    #define ME_IOTO_PRODUCT 1
#endif
#ifndef ME_PROFILE
    #define ME_PROFILE "default"
#endif
#ifndef ME_TUNE_SIZE
    #define ME_TUNE_SIZE 1
#endif

/* Miscellaneous */
#ifndef ME_MAJOR_VERSION
    #define ME_MAJOR_VERSION 2
#endif
#ifndef ME_MINOR_VERSION
    #define ME_MINOR_VERSION 5
#endif
#ifndef ME_PATCH_VERSION
    #define ME_PATCH_VERSION 0
#endif
#ifndef ME_VNUM
    #define ME_VNUM 200050000
#endif

/* Components */
#ifndef ME_COM_CC
    #define ME_COM_CC 1
#endif
#ifndef ME_COM_DB
    #define ME_COM_DB 1
#endif
#ifndef ME_COM_IOTO
    #define ME_COM_IOTO 1
#endif
#ifndef ME_COM_JSON
    #define ME_COM_JSON 1
#endif
#ifndef ME_COM_LIB
    #define ME_COM_LIB 1
#endif
#ifndef ME_COM_LINK
    #define ME_COM_LINK 1
#endif
#ifndef ME_COM_MBEDTLS
    #define ME_COM_MBEDTLS 0
#endif
#ifndef ME_COM_MQTT
    #define ME_COM_MQTT 1
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
#ifndef ME_COM_VXWORKS
    #define ME_COM_VXWORKS 0
#endif
#ifndef ME_COM_WEB
    #define ME_COM_WEB 1
#endif