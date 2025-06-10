/*
    mqtt.h -- Header for the MQTT library.

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
                                for the duration of the callback. Do not store this pointer for later use. */

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
    int topicSize;         /**< Size of topic */
    char *data;            /**< Published message */
    int dataSize;          /**< Size of data */
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
    ssize willMsgSize;      /**< Size of will message */

    int nextId;             /**< Next message ID */
    int mask;               /**< R library wait event mask */
    int msgTimeout;         /**< Message timeout for retransmit */
    int maxMessage;         /**< Maximum message size */
    Ticks keepAlive;        /**< Server side keep alive duration in seconds */
    Ticks timeout;          /**< Inactivity timeout for on-demand connections */
    Ticks lastActivity;     /**< Time of last I/O activity */

    uint subscribedApi : 1; /**< Reserved */
    uint connected : 1;     /**< Mqtt is currently connected flag */
    uint processing : 1;    /**< ProcessMqtt is running */
    uint freed : 1;         /**< Safe free detection */

    Ticks throttle;         /**< Throttle delay in msec */
    Ticks throttleLastPub;  /**< Time of last publish or throttle */
    Ticks throttleMark;     /**< Throttle sending until Time */

    char *password;         /**< Username for connect */
    char *username;         /**< Password for connect */
} Mqtt;

/**
    Allocate an MQTT object
    @param clientId Unique client identifier string.
    @param proc Event notification callback procedure.
    @stability Evolving
 */
PUBLIC Mqtt *mqttAlloc(cchar *clientId, MqttEventProc proc);

/**
   Establish a session with the MQTT broker.
   @description This call established a new MQTT connection to the broker using the supplied
        socket. The MQTT object keeps a reference to the socket. If the socket is closed or
        freed by the caller, the caller must call mqttDisconnect.
   @param mq The Mqtt object.
   @param sock The underlying socket to use for communications.
   @param flags Additional MqttConnectFlags to use when establishing the connection.
        These flags are for forcing the session to start clean: MQTT_CONNECT_CLEAN_SESSION,
        the QOS level to publish the will and testament messages with, and whether
        or not the broker should retain the will_message, MQTT_CONNECT_WILL_RETAIN.
   @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
   @returns Zero if successful.
   @stability Evolving
 */
PUBLIC int mqttConnect(Mqtt *mq, RSocket *sock, int flags, MqttWaitFlags waitFlags);

/**
    Send a disconnection packet
    @description This will not close the socket. The peer, upon receiving the disconnection
        will close the connection.
    @returns Zero if successful.
    @stability Evolving
 */
PUBLIC int mqttDisconnect(Mqtt *mq);

/**
    Free an Mqtt instance
    @param mq Mqtt instance allocated via #mqttAlloc
 */
PUBLIC void mqttFree(Mqtt *mq);

/**
    Returns an error message for error code, error.
    @param mq Mqtt object.
    @return The associated error message.
    @stability Evolving
 */
PUBLIC cchar *mqttGetError(struct Mqtt *mq);

/**
    Return the time of last I/O activity
    @return The time in Ticks of the last I/O.
    @stability Evolving
 */
PUBLIC Ticks mqttGetLastActivity(Mqtt *mq);

/**
    Get the number of messages in the queue
    @param mq The MQTT mq.
    @return The number of messages in the queue
    @stability Evolving
 */
PUBLIC int mqttMsgsToSend(Mqtt *mq);

/**
    Ping the broker.
    @param mq The MQTT mq.
    @returns Zero if successful.
    @stability Evolving
 */
PUBLIC int mqttPing(Mqtt *mq);

/**
    Publish an application message to the MQTT broker
    @param mq The Mqtt object.
    @param msg The data to be published.
    @param size The size of application_message in bytes.
    @param qos Quality of service. 0, 1, or 2.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
    @param topic Printf style topic string.
    @param ... Topic args.
    @returns Zero if successful.
 */
PUBLIC int mqttPublish(Mqtt *mq, cvoid *msg, ssize size, int qos, MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Publish a retained message to the MQTT broker
    @param mq The Mqtt object.
    @param msg The data to be published.
    @param size The size of application_message in bytes.
    @param qos Quality of service. 0, 1, or 2.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
    @param topic Printf style topic string.
    @param ... Topic args.
    @returns Zero if successful.
    @stability Evolving
 */
PUBLIC int mqttPublishRetained(Mqtt *mq, cvoid *msg, ssize size, int qos,
                               MqttWaitFlags waitFlags, cchar *topic, ...);

/*
    Define the username and password to use when connecting
    @param username The username to use when establishing the session with the MQTT broker.
        Set to NULL if a username is not required.
    @param password The password to use when establishing the session with the MQTT broker.
        Set to NULL if a password is not required.
    @stability Evolving
 */
PUBLIC int mqttSetCredentials(Mqtt *mq, cchar *username, cchar *password);

/**
    Set the maximum message size
    @description AWS supports a smaller maximum message size
    @param mq The MQTT mq.
    @param size The maximum message size
    @stability Evolving
 */
PUBLIC void mqttSetMessageSize(Mqtt *mq, int size);

/**
    Set the will and testament message
    @param mq The MQTT mq.
    @param topic Will message topic.
    @param msg Message to send
    @param length Message size.
    @stability Evolving
 */
PUBLIC int mqttSetWill(Mqtt *mq, cchar *topic, cvoid *msg, ssize length);

/**
    Subscribe to a topic.
    @param mq Mqtt object.
    @param callback Function to invoke on receipt of messages.
    @param maxQos Maximum quality of service message to receive.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
    @param topic Printf style topic string.
    @param ... Topic args.
    @returns Zero if successful.
    @stability Evolving
 */
PUBLIC int mqttSubscribe(Mqtt *mq, MqttCallback callback, int maxQos,
                         MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Perform a master subscription.
    @description To minimize the number of active MQTT subscriptions, this call can establish a
    master subscription. Subsequent subscriptions using this master topic as a prefix will
    not incurr an MQTT protocol subscription. But will be processed off the master subscription locally.
    @param mq Mqtt object.
    @param maxQos Maximum quality of service message to receive. This is used for all local subscriptions
        using this master.
    @param waitFlags Wait flags. Set to MQTT_WAIT_NONE, MQTT_WAIT_SENT or MQTT_WAIT_ACK.
    @param topic Printf style topic string.
    @param ... Topic args.
    @returns Zero if successful.
    @stability Prototype
 */
PUBLIC int mqttSubscribeMaster(Mqtt *mq, int maxQos, MqttWaitFlags waitFlags, cchar *topic, ...);

/**
    Unsubscribe from a topic.
    @description This call will unsubscribe from a topic. If the topic is a master topic,
    it will be unsubscribed from locally.
    @param mq The MQTT mq.
    @param topic The name of the topic to unsubscribe from.
    @param wait Wait flags
    @returns Zero if successful.
    @stability Evolving
 */
PUBLIC int mqttUnsubscribe(Mqtt *mq, cchar *topic, MqttWaitFlags wait);

/**
    Unsubscribe from a master topic.
    @param mq The MQTT mq.
    @param topic The name of the topic to unsubscribe from.
    @param wait Wait flags
    @returns Zero if successful.
 */
PUBLIC int mqttUnsubscribeMaster(Mqtt *mq, cchar *topic, MqttWaitFlags wait);

/**
    Set the keep-alive timeout
    @param mq The MQTT mq.
    @param keepAlive Time to wait in milliseconds before sending a keep-alive message
    @stability Evolving
 */
PUBLIC void mqttSetKeepAlive(Mqtt *mq, Ticks keepAlive);

/**
    Set the idle connection timeout
    @param mq The MQTT mq.
    @param timeout Time to wait in milliseconds before closing the connection
    @stability Evolving
 */
PUBLIC void mqttSetTimeout(Mqtt *mq, Ticks timeout);

/**
    Return true if the MQTT instance is connected to a peer
    @param mq The MQTT mq.
    @return True if connected
    @stability prototype
 */
PUBLIC bool mqttIsConnected(Mqtt *mq);

/**
    Throttle sending
    @stability Internal
 */
PUBLIC void mqttThrottle(Mqtt *mq);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_MQTT */
#endif
