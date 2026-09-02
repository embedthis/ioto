/*
    ioto.h - Ioto Device Agent API

    Complete IoT solution combining multiple embedded C libraries into a unified agent for local and cloud-based
    device management. This header provides the main API for the Ioto Device Agent, including cloud connectivity,
    database services, web server, MQTT client, and device provisioning.

    The Ioto Device Agent is designed for embedded IoT applications and provides:
    - Cloud connectivity and device management
    - Embedded database with synchronization
    - HTTP/HTTPS web server
    - MQTT client protocol
    - Device provisioning and registration
    - Real-time messaging and state management
    - AI service integration
    - AWS IoT Core integration

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_IOTO_H
#define _h_IOTO_H 1

/********************************* Configuration ******************************/

/*
    Default service configuration
 */
#include "config.h"

/********************************* Dependencies *******************************/
/*
    Enable required dependent services.

    SERVICES_CLOUD must be explicitly enabled via config.h or -DSERVICES_CLOUD=1.
    It is NOT auto-derived from dependent services.

    SERVICES_MQTT is the Ioto Cloud MQTT broker and service. It is a cloud sub-service
    and requires SERVICES_CLOUD. Customers who want generic MQTT can use the low-level
    MQTT protocol library (paks/mqtt) directly without enabling SERVICES_MQTT.

    When enabled, SERVICES_CLOUD auto-enables its dependencies:
      CLOUD -> MQTT, PROVISION, REGISTER, SSL, URL, CRON
      SYNC  -> CLOUD, DATABASE, MQTT
      LOGS  -> KEYS
 */
#if SERVICES_LOGS
    #undef SERVICES_KEYS
    #define SERVICES_KEYS 1
#endif

#if SERVICES_SYNC
    #undef SERVICES_CLOUD
    #define SERVICES_CLOUD    1
    #undef SERVICES_DATABASE
    #define SERVICES_DATABASE 1
    #undef SERVICES_MQTT
    #define SERVICES_MQTT     1
    #define ME_COM_DB         1
#endif

#if SERVICES_CLOUD
    #undef SERVICES_MQTT
    #define SERVICES_MQTT 1
    #define ME_COM_MQTT   1
#endif

#if SERVICES_CLOUD
    #undef SERVICES_PROVISION
    #define SERVICES_PROVISION 1
#endif

#if SERVICES_PROVISION || SERVICES_CLOUD
    #undef SERVICES_REGISTER
    #define SERVICES_REGISTER 1
#endif

#if SERVICES_CLOUD || SERVICES_PROVISION || SERVICES_REGISTER
    #undef SERVICES_SSL
    #define SERVICES_SSL 1
    #undef ME_COM_SSL
    #define ME_COM_SSL   1
    #undef SERVICES_URL
    #define SERVICES_URL 1
    #define ME_COM_URL   1
#endif

#if SERVICES_MQTT || SERVICES_UPDATE
#define SERVICES_CRON    1
#endif

/*
    Enable required platform services
 */
#undef ME_COM_JSON
#define ME_COM_JSON  1
#undef ME_COM_OSDEP
#define ME_COM_OSDEP 1
#undef ME_COM_R
#define ME_COM_R     1
#undef ME_COM_UCTX
#define ME_COM_UCTX  1

/********************************** Includes **********************************/

#include "r.h"
#include "json.h"
#include "crypt.h"
#include "db.h"
#include "mqtt.h"
#include "url.h"
#include "web.h"
#include "websock.h"
#include "openai.h"

/*********************************** Defines **********************************/

#ifdef __cplusplus
extern "C" {
#endif

/*
    These are all in the config directory
 */
#if ESP32
    #define IO_STATE_DIR  "/state"                 /**< State directory */
#else
    #define IO_STATE_DIR  "state"                  /**< State directory */
#endif
#define IO_CONFIG_FILE    "@config/ioto.json5"     /**< Primary Ioto config file */
#define IO_DEVICE_FILE    "@config/device.json5"   /**< Name of the device identification config file */
#define IO_LOCAL_FILE     "@config/local.json5"    /**< Development overrides */
#define IO_PROVISION_FILE "@config/provision.json5"/**< Name of the device provisioning state file */
#define IO_WEB_FILE       "@config/web.json5"      /**< Name of the web server config file */
#define IO_CERTIFICATE    "@certs/ioto.crt"        /**< Name of the AWS thing certificate file */
#define IO_KEY            "@certs/ioto.key"        /**< Name of the AWS thing key file */
#define IO_SHADOW_FILE    "@db/shadow.json5"       /**< Name of the persisted AWS shadow state file */
#define IO_LOG_FILE       "ioto.log"               /**< Name of the ioto log file */

#ifndef IO_MAX_URL
    #define IO_MAX_URL    256                      /**< Sanity length of a URL */
#endif
#define IO_MESSAGE_SIZE   128 * 1024 * 1024        /**< Maximum AWS MQTT message size (reduced) */
#define IO_REPROVISION    (3600)                   /**< Time to wait before reprovisioning after blocked connection*/

struct IotoLog;

/************************************* Ioto ***********************************/
/**
    Ioto Device Agent control structure.
    @description Main control structure that contains all the state and configuration for the Ioto Device Agent.
    This structure holds references to all enabled services including database, web server, MQTT client,
    and cloud connectivity components. The global instance is accessible via the 'ioto' variable.
    @stability Evolving
 */
typedef struct Ioto {
    Json *config;              /**< Configuration */
    Json *properties;          /**< Properties for templates */

#if SERVICES_DATABASE
    Db *db;                    /**< Structured state database */
#endif
#if SERVICES_WEB
    WebHost *webHost;          /**< Web server host */
#endif

    char *logDir;              /**< Directory for Ioto log files */
    char *app;                 /**< App name */
    char *product;             /**< Product ID Token */
    char *profile;             /**< Run profile. Defaults to ioto.json5:profile (dev, prod) */
    char *version;             /**< Your software version number (not Ioto version) */
    char *cmdConfigDir;        /**< Command line override directory for config files */
    char *cmdListen;           /**< Command line override web server listen endpoints */
    char *cmdStateDir;         /**< Command line override directory for state files */
    char *cmdSync;             /**< Command line override directory for state files */
    cchar *cmdId;              /**< Command line override claim ID */
    cchar *cmdIotoFile;        /**< Command line override path for the ioto.json5 config file */
    cchar *cmdProfile;         /**< Command line override profile */
    cchar *cmdProduct;         /**< Command line override Product ID Token */
    cchar *cmdAIShow;          /**< Command line override for AI request/response trace */
    cchar *cmdWebShow;         /**< Command line override for web request/response trace */
    bool cmdReset : 1;         /** Command line reset */

    bool aiService : 1;        /** AI service */
    bool dbService : 1;        /** Embedded database service */
    bool mqttService : 1;      /** MQTT service */
    bool nosave : 1;           /** Do not save. i.e. run in-memory */
    bool noSaveDevice : 1;     /** Do not save device registration when sourced from environment variables*/
    bool ready : 1;            /** Ioto initialized and ready (may not be connected to the cloud) */
    bool testService : 1;      /** Test service */
    bool updateService : 1;    /** Update service */
    bool webService : 1;       /** Web server */


#if SERVICES_CLOUD || DOXYGEN
    bool cloudService : 1;     /** Cloud meta-service */
    bool cloudReady : 1;       /** Connected and synced to the cloud */
    bool connected : 1;        /** Connected to the cloud over MQTT, but may not be synced */
    bool keyService : 1;       /** AWS IAM key generation */
    bool logService : 1;       /** Log file ingest to CloudWatch logs */
    bool registered : 1;       /** Device has been registered */
    bool provisioned : 1;      /** Provisioned with the cloud */
    bool provisionService : 1; /** Cloud provisioning service */
    bool registerService : 1;  /** Device registration service */
    bool shadowService : 1;    /** AWS IoT core shadows */
    bool syncService : 1;      /** Sync device state to AWS */

    char *serializeService;    /** Manufacturing serialization (factory, auto, none) */
    cchar *cmdBuilder;         /**< Command line override builder API endpoint */
    char *builder;             /**< Builder API endpoint */
    char *id;                  /**< Claim ID */
    RList *logs;               /**< Log file ingestion list */

    char *instance;            /**< EC2 instance */
    char *awsRegion;           /**< Default AWS region */
    char *awsAccess;           /**< AWS temp creds */
    char *awsSecret;           /**< AWS cred secret */
    char *awsToken;            /**< AWS cred token */
    Time awsExpires;           /**< AWS cred expiry */
    Time blockedUntil;         /**< Time to wait before reprovisioning after blocked connection */

    cchar *cmdAccount;         /**< Command line override owning manager account for self-claiming */
    cchar *cmdCloud;           /**< Command line override builder cloud for self-claiming */

    char *account;             /**< Owning manager accountId (provision.json5) */
    char *api;                 /**< Device cloud API endpoint */
    char *apiToken;            /**< Device cloud API authentication token */
    char *cloud;               /**< Builder cloud ID */
    char *cloudType;           /**< Type of cloud hosting: "hosted", "dedicated" */
    char *endpoint;            /**< Device cloud API endpoint */
    REvent shadowEvent;        /**< Shadow event */
    char *shadowName;          /**< AWS IoT shadow name */
    char *shadowTopic;         /**< AWS IoT shadow topic */

    REvent scheduledConnect;   /**< Schedule connection event */

#if SERVICES_MQTT
    Mqtt *mqtt;                /**< Mqtt object */
    RSocket *mqttSocket;       /**< Mqtt socket */
    RList *rr;                 /**< MQTT request / response list */
    int mqttErrors;            /** MQTT connection errors */
#endif

#if SERVICES_SHADOW
    Json *shadow;              /**< Shadow state */
#endif

#if SERVICES_SYNC
    Ticks syncDue;             /**< When due to emit sync changes */
    REvent syncEvent;          /**< Schedule synchronization event */
    ssize maxSyncSize;         /**< Limit of buffered database changes */
    ssize syncSize;            /**< Size of buffered database changes */
    RHash *syncHash;           /**< Hash of database change records */
    FILE *syncLog;             /**< Sync log file descriptor */
    char *lastSync;            /**< Last item sync time */
#endif

    struct IotoLog *log;       /**< Cloud Watch Log object */
#endif /* SERVICES_CLOUD || DOXYGEN */
} Ioto;

/**
    Global Ioto Device Agent instance.
    @description Main global instance of the Ioto Device Agent control structure. This is initialized
    by calling ioInit() and contains all the runtime state, configuration, and service references.
    Applications access this global instance to interact with Ioto services.
    @stability Evolving
 */
PUBLIC_DATA Ioto *ioto;

/**
    Allocate the Ioto global object.
    @description Creates and initializes a new Ioto control structure. This is typically called internally
    by ioInit() and should not be called directly by applications.
    @return A newly allocated Ioto structure or NULL on allocation failure.
    @stability Internal
 */
PUBLIC Ioto *ioAlloc(void);

/**
    Free the Ioto global object.
    @description Releases all resources associated with the global Ioto instance. This is typically called
    internally by ioTerm() and should not be called directly by applications.
    @stability Internal
 */
PUBLIC void ioFree(void);

/**
    Initialize the Ioto Device Agent.
    @description Initializes the Ioto Device Agent by creating the global ioto instance, loading configuration,
    and starting all enabled services. This must be called before using any other Ioto APIs.
    @stability Evolving
 */
PUBLIC void ioInit(void);

/**
    Terminate the Ioto Device Agent.
    @description Shuts down all Ioto services, disconnects from the cloud, and releases all resources.
    This should be called when the application is shutting down.
    @stability Evolving
 */
PUBLIC void ioTerm(void);

/**
    User configuration entry point.
    @description The ioConfig function is invoked when Ioto has read its configuration into ioto->config
        and before Ioto initializes services. Users can provide their own ioConfig function and link with
        the Ioto library. Ioto will then invoke the user's ioConfig for custom configuration.
    @param config The loaded configuration object
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
    @see ioStart, ioStop
 */
PUBLIC int ioConfig(Json *config);

/**
    User start entry point.
    @description The ioStart function is invoked when Ioto is fully initialized and ready to start.
        Users can provide their own ioStart and ioStop functions and link with the Ioto library. Ioto will then
        invoke the user's ioStart for custom initialization.
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
    @see ioStop, ioConfig
 */
PUBLIC int ioStart(void);

/**
    User stop entry point.
    @description The ioStop function is invoked when Ioto is shutting down.
        Users can provide their own ioStop function and link with the Ioto library. Ioto will then
        invoke the user's ioStop for custom shutdown cleanup.
    @stability Evolving
    @see ioConfig, ioStart
 */
PUBLIC void ioStop(void);

/**
    Get a configuration value as a string.
    @description This call is a thin wrapper over jsonGet(ioto->config, ...) that retrieves a configuration
        value from the loaded Ioto configuration files and returns it as a string.
    @param key Property name to search for. This may include dots for nested properties (e.g., "settings.mode").
    @param defaultValue Default value to return if the key is not defined. If NULL, an empty string is returned.
    @return String reference into the config store or defaultValue if not defined. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *ioGetConfig(cchar *key, cchar *defaultValue);

/**
    Get a configuration value as an integer.
    @description Retrieves an integer configuration value from the loaded Ioto configuration files.
    @param key Property name to search for. This may include dots for nested properties (e.g., "settings.mode").
    @param defaultValue Default value to return if the key is not defined or cannot be converted to an integer.
    @return The integer value from the config store or defaultValue if not defined.
    @stability Evolving
 */
PUBLIC int ioGetConfigInt(cchar *key, int defaultValue);

#if SERVICES_DATABASE
/**
    Restart the database service.
    @description Stops and restarts the embedded database service. This will close all database
        connections and reinitialize the database subsystem.
    @stability Evolving
 */
PUBLIC void ioRestartDb(void);
#endif

#if SERVICES_WEB
/**
    Restart the web server service.
    @description Stops and restarts the web server service. This will close all client connections
        and reinitialize the web server subsystem.
    @stability Evolving
 */
PUBLIC void ioRestartWeb(void);
#endif

/********************************* Web Extensions *****************************/
/*
    These APIs extend the Web API with database support
 */
#if SERVICES_WEB
#if SERVICES_DATABASE

/**
    Write a database item
    @description This routine serialize a database item into JSON. It will NOT call webFinalize.
    @param web Web object
    @param item Database item
    @return The number of bytes written.
    @stability Deprecated
 */
PUBLIC ssize webWriteItem(Web *web, const DbItem *item);

/**
    Write a grid of database items as part of a web response
    @description This routine serializes a database grid into JSON. It will NOT call webFinalize.
    @param web Web object
    @param items Grid of database items
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteItems(Web *web, RList *items);

/**
    Write a database item response.
    @description This routine serialize a database item into JSON and validates the item's fields
        against the web signature if defined. It will call webFinalize.
    @param web Web object
    @param item Database item
    @return The number of bytes written.
    @stability Deprecated
 */
PUBLIC ssize webWriteValidatedItem(Web *web, const DbItem *item, cchar *sigKey);

/**
    Write a grid of database items as a response.
    @description This routine serializes a database grid into JSON and validates the response
        against the web signature if defined. It will call webFinalize.
    @param web Web object
    @param items Grid of database items
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteValidatedItems(Web *web, RList *items, cchar *sigKey);

/**
    Login action routine
    @description Use the "username" and "password" web vars to validate against a user item stored in the database
        using the LocalUser model. If the user authenticates, the client will be redirected to "/" with a
        HTTP 302 status.  If the user fails to authenticate, a HTTP 401 response will be returned.
        This routine should be installed using: webAddAction(host, "/api/public/login", webLoginUser, NULL);
    @param web Web object
    @stability Evolving
 */
PUBLIC void webLoginUser(Web *web);

/**
    Logout action routine
    @description Log out a logged in user. This call will generate a redirect response to "/" with a 302 HTTP status.
        This routine should be installed using: webAddAction(host, "/api/public/logout", webLogoutUser, NULL);
    @param web Web object
    @stability Evolving
 */
PUBLIC void webLogoutUser(Web *web);
#endif
#endif

/*
    Internal APIs
 */
PUBLIC int ioInitAI(void);
PUBLIC void ioTermAI(void);
PUBLIC int ioInitConfig(void);
PUBLIC int ioInitDb(void);
PUBLIC int ioInitWeb(void);
PUBLIC void ioTermConfig(void);
PUBLIC void ioTermDb(void);
PUBLIC void ioTermWeb(void);
PUBLIC int ioGetFileMode(void);
PUBLIC char *ioExpand(cchar *s);
PUBLIC void ioSetTemplateVar(cchar *key, cchar *value);
PUBLIC int ioLoadConfig(void);
PUBLIC int ioUpdateLog(bool force);
PUBLIC Ticks cronUntil(cchar *spec, Time when);
PUBLIC Ticks cronUntilEnd(cchar *spec, Time when);

#define IOTO_PROD    0          /**< Configure trace for production (minimal) */
#define IOTO_VERBOSE 1          /**< Configure trace for development with verbose output */
#define IOTO_DEBUG   2          /**< Configure debug trace for development with very verbose output */

/**
    Initialize the Ioto runtime
    @param verbose Set to 1 to enable verbose output. Set to 2 for debug output.
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int ioStartRuntime(int verbose);

/**
    Stop the Ioto runtime
    @stability Evolving
 */
PUBLIC void ioStopRuntime(void);

/**
    Start Ioto services
    @description This routine blocks and services Ioto requests until commanded to exit via rStop()
    @param fn Start function. This function is not called. It is used to ensure the build system links with the supplied
       function only.
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int ioRun(void *fn);

#if SERVICES_CLOUD
#include "cloud.h"
#endif

#if ESP32
/**
    Initialize ESP32 WIFI
    @param ssid WIFI SSID for the network
    @param password WIFI password
    @param hostname Network hostname for the device
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int ioWIFI(cchar *ssid, cchar *password, cchar *hostname);

/**
    Initialize the Flash filesystem
    @param path Mount point for the file system
    @param storage Name of the LittleFS partition
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int ioStorage(cchar *path, cchar *storage);

/**
    Start the SNTP time service
    @param wait Set to true to wait for the time to be established.
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int ioSetTime(bool wait);
#endif

#ifdef __cplusplus
}
#endif
#endif /* _h_IOTO_H */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
