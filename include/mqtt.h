/*
    mqtt.h - MQTT client library for IoT publish/subscribe communications.

    This module provides a complete MQTT 3.1.1 client implementation
    for embedded IoT applications. Features include secure TLS connections,
    quality of service levels 0-2, retained messages, last will and testament,
    and efficient publish/subscribe operations with topic pattern matching.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_MQTT
#define _h_MQTT 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"

/*********************************** Defines **********************************/
#if ME_COM_MQTT
/**
    MQTT Protocol. A high-performance library for IoT publish/subscribe communications.
    @stability Evolving
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Mqtt;
struct MqttMsg;
struct MqttRecv;

#define MQTT_INLINE_BUF_SIZE        128                 /**< Size of inline buffer */
#define MQTT_BUF_SIZE               4096                /**< Receive buffer size */

#ifndef MQTT_KEEP_ALIVE
/*
    Note: https://docs.aws.amazon.com/general/latest/gr/iot-core.html#iot-protocol-limits
    AWS keep alive is 1200 seconds. We use a little less, so a ping can be used to keep the
    connection alive indefinitely. This modules supports on-demand connections with timeouts.
 */
    #define MQTT_KEEP_ALIVE         (20 * 60 * TPS)     /**< Default connection keep alive time */
#endif
#ifndef MQTT_TIMEOUT
    #define MQTT_TIMEOUT            (MAXINT)            /**< Default connection timeout in msec */
#endif
#ifndef MQTT_MSG_TIMEOUT
    #define MQTT_MSG_TIMEOUT        (30 * TPS)          /**< Default message timeout */
#endif
#ifndef MQTT_MAX_TOPIC_SIZE
    #define MQTT_MAX_TOPIC_SIZE     128                 /**< Max topic size */
#endif
#ifndef MQTT_MAX_MESSAGE_SIZE
    #define MQTT_MAX_MESSAGE_SIZE   256 * 1024 * 1024   /**< Max message size */
#endif
#ifndef MQTT_MAX_CLIENT_ID_SIZE
    #define MQTT_MAX_CLIENT_ID_SIZE 23
#endif
#ifndef MQTT_MAX_USERNAME_SIZE
    #define MQTT_MAX_USERNAME_SIZE  128
#endif
#ifndef MQTT_MAX_PASSWORD_SIZE
    #define MQTT_MAX_PASSWORD_SIZE  128
#endif


/**
    Protocol version 3.1.1
    @stability Internal
 */
#define MQTT_PROTOCOL_LEVEL 0x04

/**
    Message States
    @stability Internal
 */
typedef enum MqttMsgState {
    MQTT_UNSENT       = 1,   /**< Unsent */
    MQTT_AWAITING_ACK = 2,   /**< Awaiting ack */
    MQTT_COMPLETE     = 3    /**< Complete */
} MqttMsgState;

/**
    Wait flags
    @stability Evolving
 */
#define MQTT_WAIT_NONE 0x0   /**< No wait */
#define MQTT_WAIT_SENT 0x1   /**< Wait for sent */
#define MQTT_WAIT_ACK  0x2   /**< Wait for ack */
#define MQTT_WAIT_FAST 0x4   /**< Fast callback.
                                @warning The MqttRecv* passed to the callback is allocated on the stack and is only
                                   valid
                                for the duration of the callback. Do not store this pointer for later use. Also,
                                the rp->data pointer is not null terminated. */

typedef int MqttWaitFlags;

/**
    MQTT packet types
    @stability Evolving
 */
typedef enum MqttPacketType {
    MQTT_PACKET_CONNECT    = 1,
    MQTT_PACKET_CONN_ACK   = 2,
    MQTT_PACKET_PUBLISH    = 3,
    MQTT_PACKET_PUB_ACK    = 4,
    MQTT_PACKET_PUB_REC    = 5,
    MQTT_PACKET_PUB_REL    = 6,
    MQTT_PACKET_PUB_COMP   = 7,
    MQTT_PACKET_SUB        = 8,
    MQTT_PACKET_SUB_ACK    = 9,
    MQTT_PACKET_UNSUB      = 10,
    MQTT_PACKET_UNSUB_ACK  = 11,
    MQTT_PACKET_PING       = 12,
    MQTT_PACKET_PING_ACK   = 13,
    MQTT_PACKET_DISCONNECT = 14
} MqttPacketType;

/**
    Subscription acknowledgement return codes.
    @stability Internal
 */
typedef enum MqttSubackCode {
    MQTT_SUBACK_SUCCESS_MAX_QOS_0 = 0,
    MQTT_SUBACK_SUCCESS_MAX_QOS_1 = 1,
    MQTT_SUBACK_SUCCESS_MAX_QOS_2 = 2,
    MQTT_SUBACK_FAILURE           = 128
} MqttSubackCode;

/**
    Connect flags
    @stability Internal
 */
typedef enum MqttConnectFlags {
    MQTT_CONNECT_RESERVED      = 1,
    MQTT_CONNECT_CLEAN_SESSION = 2,
    MQTT_CONNECT_WILL_FLAG     = 4,
    MQTT_CONNECT_WILL_QOS_0    = (0 & 0x03) << 3,
    MQTT_CONNECT_WILL_QOS_1    = (1 & 0x03) << 3,
    MQTT_CONNECT_WILL_QOS_2    = (2 & 0x03) << 3,
    MQTT_CONNECT_WILL_RETAIN   = 32,
    MQTT_CONNECT_PASSWORD      = 64,
    MQTT_CONNECT_USER_NAME     = 128,
} MqttConnectFlags;

/**
    Publish flags
    @stability Evolving
 */
typedef enum MqttPubFlags {
    MQTT_QOS_FLAGS_0    = ((0 << 1) & 6),
    MQTT_QOS_FLAGS_1    = ((1 << 1) & 6),
    MQTT_QOS_FLAGS_2    = ((2 << 1) & 6),
    MQTT_QOS_FLAGS_MASK = ((3 << 1) & 6),
    MQTT_RETAIN         = 1,
    MQTT_DUP            = 8,
} MqttPubFlags;

/**
    Return code returned in a CONN ACK packet.
    @stability Evolving
 */
typedef enum MqttConnCode {
    MQTT_CONNACK_ACCEPTED                          = 0,
    MQTT_CONNACK_REFUSED_PROTOCOL_VERSION          = 1,
    MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED       = 2,
    MQTT_CONNACK_REFUSED_SERVER_UNAVAILABLE        = 3,
    MQTT_CONNACK_REFUSED_BAD_USER_NAME_OR_PASSWORD = 4,
    MQTT_CONNACK_REFUSED_NOT_AUTHORIZED            = 5
} MqttConnCode;

/**
    Message receipt callback
    @param resp Message received structure
    @stability Evolving
 */
typedef void (*MqttCallback)(const struct MqttRecv *resp);

#define MQTT_EVENT_ATTACH     1           /**< Attach a socket */
#define MQTT_EVENT_CONNECTED  2           /**< A new connection was established */
#define MQTT_EVENT_DISCONNECT 3           /**< Connection closed */
#define MQTT_EVENT_TIMEOUT    4           /**< The idle connection has timed out */

/**
    MQTt event callback
    @param mq Mqtt object created via #mqttAlloc
    @param event Event type, set to MQTT_EVENT_CONNECT, MQTT_EVENT_DISCONNECT or MQTT_EVENT_STOPPING.
    @stability Evolving
 */
typedef void (*MqttEventProc)(struct Mqtt *mq, int event);

typedef struct MqttTopic {
    char *topic;
    cchar **segments;
    char *segbuf;
    MqttCallback callback;
    MqttWaitFlags wait;
} MqttTopic;

/**
    Fixed header of a packet
    @stability Evolving
 */
typedef struct MqttHdr {
    MqttPacketType type;
    int flags;             /**< Packet control flags */
    uint length;           /**< Size in of the variable portion after fixed header and packet length */
} MqttHdr;


/**
    A struct used to deserialize/interpret an incoming packet from the broker.
    @stability Evolving
 */
typedef struct MqttRecv {
    struct MqttHdr hdr;    /**< MQTT message fixed header */
    struct Mqtt *mq;       /**< Message queue */
    int id;                /**< Message ID */

    //  Pub
    char *topic;           /**< Topic string */
    char *data;            /**< Published message */
    size_t topicSize;      /**< Size of topic */
    size_t dataSize;       /**< Size of data */
    cuchar *start;         /**< Start of message */
    uchar dup;             /**< Set to 0 on first attempt to send packet */
    uchar qos;             /**< Quality of service */
    uchar retain;          /**< Message is retained */
    MqttTopic *matched;    /**< Matched topic */

    //  Conn Ack
    uint hasSession : 1;   /**< Connection using an existing session */
    MqttConnCode code;     /**< Connection response code */

    //  Sub Ack
    cuchar *codes;         /**< Array of return codes for subscribed topics */
    int numCodes;          /**< Size of codes */

} MqttRecv;

/**
    Mqtt message
    @stability Internal
 */
typedef struct MqttMsg {
    struct MqttMsg *next;                       /**< Next message in the queue */
    struct MqttMsg *prev;                       /**< Previous message in the queue */
    uchar inlineBuf[MQTT_INLINE_BUF_SIZE];      /**< Inline message text buffer for small message efficiency */
    uchar *buf;                                 /**< External message text buffer for large messages */
    uchar *start;                               /**< Start of message */
    uchar *end;                                 /**< End of message */
    uchar *endbuf;                              /**< End of message buffer */
    int id;                                     /**< Message sequence ID */
    int qos;                                    /**< Message quality of service */
    int hold;                                   /**< Dont free message */
    MqttWaitFlags wait;                         /**< Message wait flags */
    Ticks sent;                                 /**< Time the message was sent */
    MqttMsgState state;                         /**< Message send status */
    MqttPacketType type;                        /**< Message packet type */
    RFiber *fiber;                              /**< Message fiber to process the message */
} MqttMsg;

/**
    MQTT instance
    @stability Evolving
 */
typedef struct Mqtt {
    int error;              /**< Mqtt error flag */
    char *errorMsg;         /**< Mqtt error message */
    MqttMsg head;           /**< Head of message queue */
    RSocket *sock;          /**< Underlying socket transport */
    RBuf *buf;              /**< I/O read buffer */
    RList *topics;          /**< List of subscribed topics */
    REvent keepAliveEvent;  /**< Keep alive event */
    char *id;               /**< Client ID */
    MqttEventProc proc;     /**< Notification event callback */

    RList *masterTopics;    /**< List of master subscription topics */
    char *willTopic;        /**< Will and testament topic */
    char *willMsg;          /**< Will and testament message */
    size_t willMsgSize;     /**< Size of will message */

    int nextId;             /**< Next message ID */
    int mask;               /**< R library wait event mask */
    int msgTimeout;         /**< Message timeout for retransmit */
    size_t maxMessage;      /**< Maximum message size */
    int fiberCount;         /**< Number of fibers waiting for a message */
    Ticks keepAlive;        /**< Server side keep alive duration in seconds */
    Ticks timeout;          /**< Inactivity timeout for on-demand connections */
    Ticks lastActivity;     /**< Time of last I/O activity */

    uint subscribedApi : 1; /**< Reserved */
    uint connected : 1;     /**< Mqtt is currently connected flag */
    uint processing : 1;    /**< ProcessMqtt is running */
    uint destroyed : 1;     /**< Mqtt instance is destroyed - just for debugging */

    Ticks throttle;         /**< Throttle delay in msec */
    Ticks throttleLastPub;  /**< Time of last publish or throttle */
    Ticks throttleMark;     /**< Throttle sending until Time */

    char *password;         /**< Username for connect */
    char *username;         /**< Password for connect */
} Mqtt;

/**
    Allocate an MQTT client instance.
    @description Create a new MQTT client instance with the specified client ID and event callback.
    The client ID must be unique among all clients connecting to the same broker.
    @param clientId Unique client identifier string. Maximum length is MQTT_MAX_CLIENT_ID_SIZE.
    @param proc Optional event notification callback procedure for connection/disconnection events.
    Set to NULL if not required.
    @return Returns an allocated Mqtt instance or NULL on allocation failure.
    @stability Evolving
 */
PUBLIC Mqtt *mqttAlloc(cchar *clientId, MqttEventProc proc);

/**
    Establish a session with the MQTT broker.
    @description This call establishes a new MQTT connection to the broker using the supplied
    socket. The MQTT object keeps a reference to the socket. If the socket is closed or
    freed by the caller, the caller must call mqttDisconnect first. This function must be
    called before any publish or subscribe operations.
    @param mq The Mqtt object allocated via mqttAlloc.
    @param sock The underlying socket to use for communications. This socket must be connected to an MQTT broker.
    @param flags Additional MqttConnectFlags to use when establishing the connection.
    These flags control session behavior: MQTT_CONNECT_CLEAN_SESSION to start fresh,
    QOS levels for will messages, and MQTT_CONNECT_WILL_RETAIN for retained will messages.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttConnect(Mqtt *mq, RSocket *sock, int flags, MqttWaitFlags waitFlags);

/**
    Send a disconnection packet to the MQTT broker.
    @description This sends a DISCONNECT packet to gracefully terminate the MQTT session.
    This will not close the underlying socket. The broker, upon receiving the disconnection
    packet, will close the connection. Call this before freeing the socket or MQTT instance.
    @param mq The Mqtt object.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttDisconnect(Mqtt *mq);

/**
    Free an MQTT client instance.
    @description Release all resources associated with the MQTT instance. This will automatically
    disconnect from the broker if still connected and free all subscriptions and queued messages.
    @param mq Mqtt instance allocated via mqttAlloc. Pass NULL to ignore.
    @stability Evolving
 */
PUBLIC void mqttFree(Mqtt *mq);

/**
    Get the last error message for the MQTT instance.
    @description Returns a human-readable error message describing the last error that occurred
    on this MQTT instance. This is useful for debugging connection and protocol issues.
    @param mq Mqtt object.
    @return The associated error message string or empty string if no error.
    @stability Evolving
 */
PUBLIC cchar *mqttGetError(struct Mqtt *mq);

/**
    Return the time of last I/O activity.
    @description Get the timestamp of the last network I/O activity on the MQTT connection.
    This is useful for implementing connection monitoring and keep-alive logic.
    @param mq The Mqtt object.
    @return The time in Ticks of the last I/O activity.
    @stability Evolving
 */
PUBLIC Ticks mqttGetLastActivity(Mqtt *mq);

/**
    Get the number of messages pending transmission.
    @description Returns the count of messages in the outbound queue waiting to be sent
    or acknowledged by the broker. This is useful for flow control and monitoring.
    @param mq The MQTT object.
    @return The number of messages in the transmission queue.
    @stability Evolving
 */
PUBLIC int mqttMsgsToSend(Mqtt *mq);

/**
    Send a ping request to the broker.
    @description Send a PINGREQ packet to the broker to test connectivity and reset
    the keep-alive timer. The broker will respond with a PINGRESP packet.
    @param mq The MQTT object.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttPing(Mqtt *mq);

/**
    Publish an application message to the MQTT broker.
    @description Publish a message to the specified topic. The MQTT instance must be
    connected to a broker via mqttConnect before calling this function.
    @param mq The Mqtt object.
    @param msg The data to be published. Can be binary data.
    @param size The size of the message in bytes. Maximum size is MQTT_MAX_MESSAGE_SIZE.
    @param qos Quality of service level: 0 (at most once), 1 (at least once), or 2 (exactly once).
    If QOS 0 is used, MQTT_WAIT_ACK will be ignored.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @param topic Printf-style topic string. Maximum length is MQTT_MAX_TOPIC_SIZE.
    @param ... Topic formatting arguments.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttPublish(Mqtt *mq, cvoid *msg, size_t size, int qos, MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Publish a retained message to the MQTT broker.
    @description Publish a message with the retain flag set. Retained messages are stored
    by the broker and delivered to new subscribers immediately upon subscription.
    The MQTT instance must be connected to a broker via mqttConnect.
    @param mq The Mqtt object.
    @param msg The data to be published. Can be binary data.
    @param size The size of the message in bytes. Maximum size is MQTT_MAX_MESSAGE_SIZE.
    @param qos Quality of service level: 0 (at most once), 1 (at least once), or 2 (exactly once).
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @param topic Printf-style topic string. Maximum length is MQTT_MAX_TOPIC_SIZE.
    @param ... Topic formatting arguments.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttPublishRetained(Mqtt *mq, cvoid *msg, size_t size, int qos,
                               MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Set authentication credentials for broker connection.
    @description Define the username and password to use when connecting to the MQTT broker.
    These credentials must be set before calling mqttConnect if the broker requires authentication.
    @param mq The Mqtt object.
    @param username The username to use when establishing the session with the MQTT broker.
    Set to NULL if a username is not required. Maximum length is MQTT_MAX_USERNAME_SIZE.
    @param password The password to use when establishing the session with the MQTT broker.
    Set to NULL if a password is not required. Maximum length is MQTT_MAX_PASSWORD_SIZE.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttSetCredentials(Mqtt *mq, cchar *username, cchar *password);

/**
    Set the maximum message size for this MQTT instance.
    @description Configure the maximum allowed message size for publish operations.
    Some brokers like AWS IoT have smaller limits than the default. This setting
    helps prevent oversized messages from being rejected.
    @param mq The MQTT object.
    @param size The maximum message size in bytes. Must be positive and less than MQTT_MAX_MESSAGE_SIZE.
    @stability Evolving
 */
PUBLIC void mqttSetMessageSize(Mqtt *mq, size_t size);

/**
    Set the last will and testament message.
    @description Configure a message that the broker will publish if this client
    disconnects unexpectedly. The will message is sent when the broker detects
    an abnormal disconnection (network failure, client crash, etc.).
    @param mq The MQTT object.
    @param topic Topic where the will message will be published. Maximum length is MQTT_MAX_TOPIC_SIZE.
    @param msg Will message data. Can be binary data.
    @param length Size of the will message in bytes.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttSetWill(Mqtt *mq, cchar *topic, cvoid *msg, size_t length);

/**
    Subscribe to a topic pattern.
    @description Subscribe to receive messages published to topics matching the specified pattern.
    The MQTT instance must be connected to a broker via mqttConnect. Topic patterns support
    MQTT wildcards: '+' for single level, '#' for multi-level.
    @param mq Mqtt object.
    @param callback Function to invoke when messages are received on this topic. Set to NULL
    to use a default handler.
    @param maxQos Maximum quality of service level to accept: 0, 1, or 2.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @param topic Printf-style topic pattern string supporting MQTT wildcards.
    @param ... Topic formatting arguments.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttSubscribe(Mqtt *mq, MqttCallback callback, int maxQos,
                         MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Establish a master subscription for efficient topic management.
    @description To minimize the number of active MQTT protocol subscriptions, this call
    establishes a master subscription. Subsequent local subscriptions using this master
    topic as a prefix will not incur additional MQTT protocol subscriptions but will be
    processed locally off the master subscription. This is useful for applications with
    many related topic subscriptions.
    @param mq Mqtt object.
    @param maxQos Maximum quality of service level to accept: 0, 1, or 2. This QOS level
    applies to all local subscriptions using this master.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @param topic Printf-style master topic pattern string.
    @param ... Topic formatting arguments.
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttSubscribeMaster(Mqtt *mq, int maxQos, MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Unsubscribe from a topic.
    @description Remove a subscription for the specified topic pattern. If the topic
    is a local subscription under a master topic, it will be removed locally without
    affecting the master subscription.
    @param mq The MQTT object.
    @param topic The topic pattern to unsubscribe from. Must match a previously subscribed topic.
    @param wait Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttUnsubscribe(Mqtt *mq, cchar *topic, MqttWaitFlags wait);

/**
    Unsubscribe from a master topic.
    @description Remove a master subscription and all associated local subscriptions.
    This will send an UNSUBSCRIBE packet to the broker for the master topic.
    @param mq The MQTT object.
    @param topic The master topic pattern to unsubscribe from.
    @param wait Wait flags. Set to MQTT_WAIT_NONE (async), MQTT_WAIT_SENT (wait for send),
    or MQTT_WAIT_ACK (wait for broker acknowledgment).
    @return Zero if successful, negative on error.
    @stability Evolving
 */
PUBLIC int mqttUnsubscribeMaster(Mqtt *mq, cchar *topic, MqttWaitFlags wait);

/**
    Set the keep-alive timeout interval.
    @description Configure the maximum time interval between client messages to the broker.
    If no messages are sent within this period, a PINGREQ packet will be sent automatically
    to maintain the connection.
    @param mq The MQTT object.
    @param keepAlive Time interval in milliseconds before sending a keep-alive message.
    Set to 0 to disable keep-alive.
    @stability Evolving
 */
PUBLIC void mqttSetKeepAlive(Mqtt *mq, Ticks keepAlive);

/**
    Set the idle connection timeout.
    @description Configure how long the connection can remain idle before being automatically
    closed. This is useful for on-demand connections that should disconnect when not in use.
    @param mq The MQTT object.
    @param timeout Time to wait in milliseconds before closing an idle connection.
    Set to MAXINT to disable automatic disconnection.
    @stability Evolving
 */
PUBLIC void mqttSetTimeout(Mqtt *mq, Ticks timeout);

/**
    Check if the MQTT instance is connected to a broker.
    @description Test whether the MQTT client currently has an active connection
    to an MQTT broker and can send/receive messages.
    @param mq The MQTT object.
    @return True if connected to a broker, false otherwise.
    @stability Prototype
 */
PUBLIC bool mqttIsConnected(Mqtt *mq);

/**
    Enable transmission throttling.
    @description Temporarily throttle message transmission to prevent overwhelming
    the broker or network. This is used internally for flow control.
    @param mq The MQTT object.
    @stability Internal
 */
PUBLIC void mqttThrottle(Mqtt *mq);

/**
    Check if there are messages in the transmission queue.
    @description Determine if there are pending messages waiting to be processed
    or transmitted to the broker.
    @param mq The MQTT object.
    @return True if messages are queued, false if queue is empty.
    @stability Internal
 */
PUBLIC bool mqttCheckQueue(Mqtt *mq);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_MQTT */
#endif
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */