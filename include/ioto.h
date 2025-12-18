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
    Configuration generated from ioto.json5 services
 */
#include "config.h"

/********************************* Dependencies *******************************/
/*
    Enable required and dependent services
 */
#ifndef SERVICES_CLOUD
    #if SERVICES_KEYS || \
    SERVICES_LOGS || \
    SERVICES_MQTT || \
    SERVICES_PROVISION || \
    SERVICES_SHADOW || \
    SERVICES_UPDATE || \
    SERVICES_SYNC
        #undef SERVICES_CLOUD
        #define SERVICES_CLOUD 1
    #endif
#else
#endif

#if SERVICES_LOGS
    #undef SERVICES_KEYS
    #define SERVICES_KEYS 1
#endif

#if SERVICES_SYNC
    #undef SERVICES_DATABASE
    #define SERVICES_DATABASE 1
    #undef SERVICES_MQTT
    #define SERVICES_MQTT     1
    #define ME_COM_DB         1
#endif

#if SERVICES_CLOUD
    #undef SERVICES_MQTT
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

#if SERVICES_CLOUD || SERVICES_PROVISION || defined(SERVICES_SERIALIZE) || SERVICES_REGISTER
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

#if SERVICES_SHADOW
    Json *shadow;              /**< Shadow state */
#endif
#if SERVICES_DATABASE
    Db *db;                    /**< Structured state database */
#endif
#if SERVICES_WEB
    WebHost *webHost;          /**< Web server host */
#endif
#if SERVICES_MQTT
    Mqtt *mqtt;                /**< Mqtt object */
    RSocket *mqttSocket;       /**< Mqtt socket */
    RList *rr;                 /**< MQTT request / response list */
    int mqttErrors;            /** MQTT connection errors */
#endif

    RList *logs;               /**< Log file ingestion list */

    char *builder;             /**< Builder API endpoint */
    char *id;                  /**< Claim ID */
    char *logDir;              /**< Directory for Ioto log files */
    char *app;                 /**< App name */
    char *product;             /**< Product ID Token */
    char *profile;             /**< Run profile. Defaults to ioto.json5:profile (dev, prod) */
    char *version;             /**< Your software version number (not Ioto version) */
    char *cmdConfigDir;        /**< Command line override directory for config files */
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
    bool cloudService : 1;     /** Cloud meta-service */
    bool cloudReady : 1;       /** Connected and synced to the cloud */
    bool connected : 1;        /** Connected to the cloud over MQTT, but may not be synced */
    bool dbService : 1;        /** Embedded database service */
    bool keyService : 1;       /** AWS IAM key generation */
    bool logService : 1;       /** Log file ingest to CloudWatch logs */
    bool mqttService : 1;      /** MQTT service */
    bool nosave : 1;           /** Do not save. i.e. run in-memory */
    bool noSaveDevice : 1;     /** Do not save device registration when sourced from environment variables*/
    bool registered : 1;       /** Device has been registered */
    bool provisioned : 1;      /** Provisioned with the cloud */
    bool provisionService : 1; /** Cloud provisioning service */
    bool ready : 1;            /** Ioto initialized and ready (may not be connected to the cloud) */
    bool registerService : 1;  /** Device registration service */
    bool shadowService : 1;    /** AWS IoT core shadows */
    bool syncService : 1;      /** Sync device state to AWS */
    bool testService : 1;      /** Test service */
    bool updateService : 1;    /** Update service */
    bool webService : 1;       /** Web server */

    char *serializeService;    /** Manufacturing serialization (factory, auto, none) */
    cchar *cmdBuilder;         /**< Command line override builder API endpoint */

#if SERVICES_CLOUD || DOXYGEN
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
#endif
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
    Invoke an Ioto REST API.
    @param url The URL path to invoke. Should not contain the host/tok portion.
    @param data The data to send
    @return The response from the API as a json object. Caller must free.
    @stability Evolving
 */
PUBLIC Json *ioAPI(cchar *url, cchar *data);

/**
    Invoke an automation on the device cloud.
    @param name The name of the automation to invoke
    @param context The context properties to pass to the automation
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int ioAutomation(cchar *name, cchar *context);

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
    Get a value from the Ioto cloud key/value Store
    @description This call retrieves a value from the Ioto cloud key/value store for this device.
    @param key String key value to assign a value in the store.
    @return value Key's string value. Caller must free.
    @stability Evolving
 */
PUBLIC char *ioGet(cchar *key);

/**
    Get a boolean value from the Ioto cloud key/value Store
    @description This call retrieves a value from the Ioto cloud key/value store for this device.
    @param key String key value to assign a value in the store.
    @return value Key's boolean value.
    @stability Evolving
 */
PUBLIC bool ioGetBool(cchar *key);

/**
    Get a numeric value from the Ioto cloud key/value Store
    @description This call retrieves a value from the Ioto cloud key/value store for this device.
    @param key String key value to assign a value in the store.
    @return value Key's numeric value.
    @stability Evolving
 */
PUBLIC double ioGetNum(cchar *key);

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

/**
    Get a metric value from the Ioto cloud.
    @description This call retrieves a metric from the Ioto cloud for this device.
    @param metric Metric name to retrieve from the Embedthis/Device namespace.
    @param dimensions JSON array of dimensions as a string. Each element is an object that defines
        the properties of that dimension. The empty object {} denotes All dimensions.
    @param statistic Statistic type to retrieve: "avg", "min", "max", "count", or "current".
    @param period Number of seconds for the statistic period.
    @return The metric value as a double, or NAN if the metric cannot be found.
    @stability Evolving
 */
PUBLIC double ioGetMetric(cchar *metric, cchar *dimensions, cchar *statistic, int period);

/**
    Check if the device is connected to the cloud
    @return true if connected, false otherwise
    @stability Evolving
 */
PUBLIC bool ioConnected(void);

/**
    Run function when connected to the cloud.
    @description This calls the supplied function immediately if already connected. Otherwise,
        when the connection to the cloud is re-established, the function will be invoked.
        If direct is true, the function may be executed inline without spawning a fiber. If not
        connected, the function will be run in a fiber when due.
    @param fn Function to invoke when connected.
    @param arg Argument to supply to the function.
    @param direct Set to true to invoke the function directly without spawning a fiber.
    @stability Evolving
 */
PUBLIC void ioOnConnect(RWatchProc fn, void *arg, bool direct);

/**
    Disable running function when connected to the cloud.
    @description Removes a previously registered connection callback function.
    @param fn Function to remove from the connection callback list.
    @param arg Argument that was supplied when the function was registered.
    @stability Evolving
 */
PUBLIC void ioOnConnectOff(RWatchProc fn, void *arg);

/**
    Set a string value in the Ioto cloud key/value store.
    @description This call defines a string value in the Ioto cloud key/value store for this device.
        Uses database sync if available, otherwise uses MQTT messaging.
    @param key String key to assign in the store.
    @param value String value to assign to the key.
    @stability Evolving
 */
PUBLIC void ioSet(cchar *key, cchar *value);

/**
    Set a boolean value in the Ioto cloud key/value store.
    @description This call defines a boolean value in the Ioto cloud key/value store for this device.
        Uses database sync if available, otherwise uses MQTT messaging.
    @param key String key to assign in the store.
    @param value Boolean value to assign to the key.
    @stability Evolving
 */
PUBLIC void ioSetBool(cchar *key, bool value);

/**
    Set a metric value in the Ioto cloud.
    @description This call defines a metric in the Ioto cloud for this device.
    @param metric Metric name to define in the Embedthis/Device namespace.
    @param value Metric value as a double.
    @param dimensions JSON array of dimensions as a string. Each element is an object that defines
        the properties of that dimension. The empty object {} denotes no dimensions.
    @param elapsed Number of seconds to buffer metric updates in the cloud before committing to the database.
        This is an optimization. Set to zero for no buffering.
    @return 0 on success, negative on error
    @stability Evolving
 */
PUBLIC int ioSetMetric(cchar *metric, double value, cchar *dimensions, int elapsed);

/**
    Set a numeric value in the Ioto cloud key/value store.
    @description This call defines a numeric value in the Ioto cloud key/value store for this device.
        Uses database sync if available, otherwise uses MQTT messaging.
    @param key String key to assign in the store.
    @param value Numeric value to assign to the key.
    @return 0 on success, negative on error
    @stability Evolving
 */
PUBLIC int ioSetNum(cchar *key, double value);

/**
    Schedule a cloud connection based on the MQTT schedule.
    @description Schedules a connection attempt to the cloud based on the configured MQTT connection schedule.
        This will initiate the connection process according to the timing parameters in the configuration.
    @stability Evolving
 */
PUBLIC void ioStartConnect(void);

#ifdef SERVICES_SERIALIZE
/**
    Device ID serialization.
    @description If the device.json5 config file does not already have a device ID, this call will
        allocate a unique device claim ID (10 character UDI) if required.
        If the ioto.json5 "services.serialize" is set to "auto", this API will dynamically create a
        random device ID. If the "serialize" setting is "factory", this call will invoke the factory
        serialization service defined via the "api.serialize" URL setting. The resultant deviceId is
        saved in the config/device.json5 file.
        WARNING: this blocks Ioto if calling the factory service.
    @stability Evolving
 */
PUBLIC void ioSerialize(void);
#endif

#if SERVICES_SYNC
/**
    Flush pending changes to the cloud.
    @description Database changes are buffered before being flushed to the cloud. This forces all
        pending changes to be sent to the cloud immediately.
    @param force Set to true to flush items that are not yet due to be synchronized.
    @stability Evolving
 */
PUBLIC void ioFlushSync(bool force);

/**
    Sync all items to the cloud.
    @description This call can be used to force a full sync-up of the local database to the cloud.
    @param timestamp Sync items updated after this time.
    @param guarantee Set to true to perform a reliable sync by waiting for the cloud to send
        a receipt acknowledgement for each item.
    @stability Evolving
 */
PUBLIC void ioSyncUp(Time timestamp, bool guarantee);

/**
    Sync items from the cloud down to the device.
    @description This call can be used to retrieve all items updated after the requested timestamp.
    @param timestamp Retrieve items updated after this time. If timestamp is negative, the call will
        retrieve items updated since the last sync.
    @stability Evolving
 */
PUBLIC void ioSyncDown(Time timestamp);

/**
    Sync items to and from the cloud.
    @description This call can be used to force a sync-up and sync-down of the local database to and from the cloud.
    @param when Retrieve items updated after this time. If when is negative, the call will sync items updated
        since the last sync.
    @param guarantee Set to true to perform a reliable sync by waiting for the cloud to send
        a receipt acknowledgement for each item.
    @stability Evolving
 */
PUBLIC void ioSync(Time when, bool guarantee);
#endif

/**
    Upload a file to the device cloud.
    @param path Path to the file to upload.
    @param buf Buffer containing the file data.
    @param len Length of the file data.
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int ioUpload(cchar *path, uchar *buf, size_t len);

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
#if SERVICES_SHADOW
/**
    Get a value from the shadow state.
    @param key Property key value. May contain dots.
    @param defaultValue Default value to return if the key is not found
    @return Returns an allocated string. Caller must free.
    @stability Evolving
 */
PUBLIC char *ioGetShadow(cchar *key, cchar *defaultValue);

/**
    Set a key value in the shadow
    @param key Property key value. May contain dots.
    @param value Value to set.
    @param save Set to true to persist immediately.
    @stability Evolving
 */
PUBLIC void ioSetShadow(cchar *key, cchar *value, bool save);

/**
    Save the shadow state immediately.
    @stability Evolving
 */
PUBLIC void ioSaveShadow(void);
#endif

/********************************* MQTT Extensions *****************************/
/**
    Issue a MQTT request and wait for a response
    @description This call sends a MQTT message to the Ioto service and waits for a response. If the response is not
       received the call before the timeout expires, the call returns NULL. This call will subscribe for incoming
       messages on the topic. Use mqttRequestFree if the app does not wish to use mqttRequest again.
    @param mq MQTT connection object
    @param data Message data to send
    @param timeout Timeout in milliseconds to wait for a response. If timeout is <= 0, a default timeout of 30 seconds
       is used.
    @param topic Printf style topic format string. The supplied topic is appended to 'ioto/device/DEVICE_ID'
        before sending.
    @param ... Topic string arguments
    @return Response message or NULL if the request fails or times out. Caller must free.
    @stability Evolving
 */
PUBLIC char *mqttRequest(Mqtt *mq, cchar *data, Ticks timeout, cchar *topic, ...);

/**
    Free MQTT subscriptions created by mqttRequest
    @description This call is optional and should only be used if you use many different topics with mqttRequest.
    @param mq MQTT connection object
    @param topic Printf style topic format string used in prior mqttRequest
    @param ... Topic string arguments
    @stability Evolving
 */
PUBLIC void mqttRequestFree(Mqtt *mq, cchar *topic, ...);

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

/*********************************** Cloud ************************************/
#if SERVICES_CLOUD || DOXYGEN

#define IO_LOG_GROUP      "ioto"                    /**< AWS log group name */
#define IO_LOG_STREAM     "agent"                   /**< AWS log stream name */
#define IO_LOG_MAX_EVENTS 1000                      /**< Max log events to buffer */
#define IO_LOG_MAX_SIZE   32767                     /**< Max size of log events to buffer */
#define IO_LOG_LINGER     5000                      /**< Delay before flushing log events to the cloud */
#define IO_SAVE_DELAY     5000                      /**< Delay before saving updated shadow state */

/************************************* AI **************************************/
/**
    Initialize the AI service
    @return Returns 0 on success, or -1 on failure
    @stability Evolving
 */
PUBLIC int ioInitAI(void);

/**
    Terminate the AI service
    @stability Evolving
 */
PUBLIC void ioTermAI(void);

/************************************* AWS *************************************/
/**
    AWS Support.
    @description This is suite of AWS helper routines that implement AWS SigV4 signed REST API requests.
    @stability Evolving
    @see aws, awsPutToS3, awsPutFileToS3, awsSign
 */

/**
    Create a set of signed headers to use with AWS SigV4 REST APIs.
    @description The AWS SDK is big and relatively slow. If you only need a few AWS APIs, you can
        use the AWS REST APIs and avoid the AWS SDK. This routine processes HTTP request parameters to create a set of
        signed HTTP headers that can be used with the URL HTTP client.
    @param region AWS Region to target
    @param service AWS Service name (s3)
    @param target AWS Target name. If not supplied, this is deduced from the service name and the
        x-amz-target header
    @param method HTTP method to utilize.
    @param path URL request path.
    @param query URL request query.
    @param body Request body data.
    @param bodyLen Length of the request body.
    @param headers Format string containing HTTP headers. This format string can use printf embedded %tokens
        that will be expanded to form the HTTP headers.
        The header format is of the form: "Key:Value\n..." with an extra new line at the end.
    @param ... Headers arguments.
    @return The HTTP headers to use with a URL HTTP client request. Caller must free.
    @stability Evolving
    @see aws, awsPutToS3, awsPutFileToS3
 */
PUBLIC char *awsSign(cchar *region, cchar *service, cchar *target, cchar *method, cchar *path,
                     cchar *query, cchar *body, size_t bodyLen, cchar *headers, ...);

/**
    Invoke an AWS API request
    @param up Url object allocated via urlAlloc.
    @param region AWS Region to target
    @param service AWS Service name. E.g. "s3".
    @param target AWS Target name. If not supplied, this is deduced from the service name and the "x-amz-target" header.
    @param body Request body data.
    @param bodyLen Length of the request body.
    @param headers Format string containing HTTP headers. This format string can use printf embedded %tokens
    that will be expanded to form the HTTP headers.
    The header format is of the form: "Key:Value\n..." with an extra new line at the end.
    @param ... Headers arguments.
    @stability Evolving
    @see awsPutToS3, awsPutFileToS3, awsSign
 */
PUBLIC int aws(Url *up, cchar *region, cchar *service, cchar *target, cchar *body, size_t bodyLen, cchar *headers, ...);

/**
    Put a data block to AWS S3
    @param region AWS Region to target
    @param bucket AWS S3 bucket name
    @param key AWS S3 bucket key (filename)
    @param data Data block to write to S3.
    @param dataLen Length of the data block.
    @stability Evolving
    @see aws, awsPutFileToS3, awsSign
 */
PUBLIC int awsPutToS3(cchar *region, cchar *bucket, cchar *key, cchar *data, size_t dataLen);

/**
    Put a file to AWS S3
    @param region AWS Region to target.
    @param bucket AWS S3 bucket name.
    @param key AWS S3 bucket key (filename). If set to null, the key is set to the filename
    @param filename File name to put to S3.
    @stability Evolving
    @see aws, awsPutToS3, awsSign
 */
PUBLIC int awsPutFileToS3(cchar *region, cchar *bucket, cchar *key, cchar *filename);

/************************************* Log *************************************/
/*
    Log to Cloud Watch logs
    @description Used to send log data to AWS Cloud Watch
    @stability Internal
 */
typedef struct IotoLog {
    char *path;                         /**< Log path name */
    char *region;                       /**< Region storing the captured log data */
    char *group;                        /**< AWS Cloud Watch Logs group name */
    char *stream;                       /**< AWS Cloud Watch Logs stream name */
    REvent event;                       /**< Buffer timeout event */
    Url *up;                            /**< Url to connect to AWS Cloud Watch */
    RList *buffers;                     /**< Queue of buffers to send */
    RBuf *buf;                          /**< Current buffer */
    RBuf *sending;                      /**< Buffer currently being sent */
    Ticks bufStarted;                   /**< Time the current buffer started storing data */
    Ticks linger;                       /**< How long to buffer data before flushing to Cloud Watch */
    int hiw;                            /**< Buffer byte count hiw */
    int max;                            /**< Buffer byte count maximum size */
    int events;                         /**< Number of events in current buffer */
    int eventsHiw;                      /**< High water mark of events to trigger flushing to CW */
    int maxEvents;                      /**< Maximum number of events in buffer - discard buffer if exceeded */
    bool create;                        /**< Create log group if it does not alraedy exist */
    char *sequence;                     /**< Next PutLogEvents sequence number, required by AWS API */
} IotoLog;

/*
    Log a message
    @param log Log capture object
    @param time Current wall clock time
    @param msg Message to log
    @return Zero if successful.
    @stability Internal
 */
PUBLIC int ioLogMessage(IotoLog *log, Time time, cchar *msg);

#if SERVICES_PROVISION
/*
    Provision a device for cloud communications
    @description Callers can invoke this API for immediate provisioning. Provisioning
        uses an exponential delay while the device has not been claimed. This increases gradually
        up to a 24 hour delay. If called while another call to ioProvision is executing, the second
        call waits for the first to complete.
    @stability Evolving
 */
PUBLIC int ioProvision();

/**
    Deprovision the device.
    @description Removes device provisioning and resets the device to an unprovisioned state.
    @stability Internal
 */
PUBLIC void ioDeprovision(void);

/**
    Connect to the cloud.
    @description Initiates a connection to the cloud services.
    @return Zero if successful, otherwise a negative error code.
    @stability Internal
 */
PUBLIC int ioConnect(void);

/**
    Handle cloud connection established.
    @description Internal callback when cloud connection is established.
    @stability Internal
 */
PUBLIC void ioOnCloudConnect(void);

/**
    Disconnect from the cloud.
    @description Terminates the connection to cloud services.
    @stability Internal
 */
PUBLIC void ioDisconnect(void);

/**
    Start the provisioning service.
    @description Initializes and starts the device provisioning service.
    @stability Internal
 */
PUBLIC void ioStartProvisioner(void);

/**
    Wake the provisioning service.
    @description Signals the provisioning service to check for pending work.
    @stability Internal
 */
PUBLIC void ioWakeProvisioner(void);

/**
    Calculate exponential backoff delay.
    @description Calculates an exponential backoff delay for retry operations.
    @param delay Current delay value.
    @param event Event to schedule.
    @return New delay value.
    @stability Internal
 */
PUBLIC Ticks ioBackoff(Ticks delay, REvent *event);

/**
    Resume from backoff delay.
    @description Resumes operation after a backoff delay.
    @param event Event that was scheduled.
    @stability Internal
 */
PUBLIC void ioResumeBackoff(REvent *event);
#endif

/*
    Internal
 */
PUBLIC IotoLog *ioAllocLog(cchar *name, cchar *region, int create, cchar *group,
                           cchar *stream, int maxEvents, int size, Ticks linger);
PUBLIC int ioEnableCloudLog(void);
PUBLIC void ioFreeLog(IotoLog *log);
PUBLIC bool ioUpdate(void);

#endif /* SERVICES_CLOUD */

/*
    Internal APIs
 */
PUBLIC int ioInitAI(void);
PUBLIC int ioInitConfig(void);
PUBLIC int ioInitCloud(void);
PUBLIC int ioInitDb(void);
PUBLIC int ioInitLogs(void);
PUBLIC int ioInitMqtt(void);
PUBLIC int ioInitProvisioner(void);
PUBLIC int ioInitShadow(void);
PUBLIC int ioInitSync(void);
PUBLIC int ioInitWeb(void);

PUBLIC void ioTermCloud(void);
PUBLIC void ioTermConfig(void);
PUBLIC void ioTermDb(void);
PUBLIC void ioTermLogs(void);
PUBLIC void ioTermMqtt(void);
PUBLIC void ioTermProvisioner(void);
PUBLIC void ioTermShadow(void);
PUBLIC void ioTermSync(void);
PUBLIC void ioTermWeb(void);

PUBLIC int ioRegister(void);
PUBLIC void ioUpdateDevice(void);
PUBLIC int ioUpdateLog(bool force);
PUBLIC int ioGetFileMode(void);
PUBLIC char *ioExpand(cchar *s);
PUBLIC void ioSetTemplateVar(cchar *key, cchar *value);
PUBLIC void ioGetKeys(void);
PUBLIC int ioLoadConfig(void);
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
