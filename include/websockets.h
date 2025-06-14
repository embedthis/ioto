/*
    websockets.h -- WebSockets Header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_WEBSOCKETS
#define _h_WEBSOCKETS 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"
#include "crypt.h"
#include "json.h"

#if ME_COM_WEBSOCKETS
/*********************************** Defines **********************************/

#ifdef __cplusplus
extern "C" {
#endif

/********************************* Web Sockets ********************************/

struct WebSocket;

#define WS_EVENT_OPEN 0             /**< WebSocket connection is open */
#define WS_EVENT_MESSAGE 1          /**< WebSocket full (or last part of) message is received */
#define WS_EVENT_PARTIAL_MESSAGE 2  /**< WebSocket partial message is received */
#define WS_EVENT_ERROR 3            /**< WebSocket error is detected */
#define WS_EVENT_CLOSE 4            /**< WebSocket connection is closed */

/**
    WebSocket callback procedure invoked when a message is received or the connection is first opened.
    @param webSocket WebSocket object
    @param event Event type
    @param buf Message buffer holding the message data. On open and close, the buffer is NULL and the length is 0.
    @param len Length of the message
    @param arg User argument
 */
typedef void (*WebSocketProc)(struct WebSocket *webSocket, int event, cchar *buf, ssize len, void *arg);

/**
    WebSocket WebSockets RFC 6455 implementation for client and server communications.
    @description WebSockets is a technology providing interactive communication between a server
        and client. Normal HTML connections follow a request / response paradigm and do not
        easily support asynchronous communications or unsolicited data pushed from the server
        to the client. WebSockets solves this by supporting bi-directional, full-duplex
        communications over persistent connections. A WebSocket connection is established over a
        standard HTTP connection and is then upgraded without impacting the original connection.
        This means it will work with existing networking infrastructure
    including firewalls and proxies.
    @see webSocketAlloc, webSocketProcess
    @stability Internal
 */
typedef struct WebSocket {

    ssize maxFrame;                        /**< Maximum frame size */
    ssize maxMessage;                      /**< Maximum message size */
    ssize maxPacket;                       /**< Maximum packet size */

    int client;                            /**< Client or server */
    int error;                             /**< Error code */
    int fin;                               /**< RX final packet */
    int frame;                             /**< Message frame state */
    int closing;                           /**< Started closing sequnce */
    int closeStatus;                       /**< Close status provided by peer */
    int inCallback;                        /**< In callback */
    int maskOffset;                        /**< Offset in dataMask */
    int needFree;                          /**< Need to free */
    int opcode;                            /**< Rx message opcode */
    int partialUTF;                        /**< Last frame had a partial UTF codepoint */
    int rxSeq;                             /**< Incoming packet number (debug only) */
    int state;                             /**< Protocol State */
    int txSeq;                             /**< Outgoing packet number (debug only) */
    int type;                              /**< Rx message accumulated type */
    int validate;                          /**< Validate UTF8 codepoints */

    ssize frameLength;                     /**< Length of the current frame */
    ssize messageLength;                   /**< Length of the current message */

    char *clientKey;                       /**< Client key */
    char *closeReason;                     /**< Reason for closure */
    char *errorMessage;                    /**< Error message for last I/O */
    char *protocol;                        /**< Selected protocol */

    void *parent;                          /**< User available parent object link */
    void *data;                            /**< User available data link */
    uchar dataMask[4];                     /**< Mask for data */

    Ticks deadline;                        /**< Timeout deadline for when the next I/O must complete */
    RSocket *sock;                         /**< Communication socket */
    Time pingPeriod;                       /**< Ping period */
    REvent pingEvent;                      /**< Ping timer event */
    REvent abortEvent;                     /**< Abort event */

    WebSocketProc callback;                /**< Open and read event callback */
    void *callbackArg;                     /**< Event callback argument */
    RFiber *fiber;                         /**< Fiber waiting for close */
    RBuf *buf;                             /**< Buffer for incoming data */
} WebSocket;

#define WS_MAGIC                    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAX_CONTROL              125    /**< Maximum bytes in control message */
#define WS_VERSION                  13     /**< Current WebSocket specification version */

#define WS_SERVER                   0      /**< Instance executing as a server */
#define WS_CLIENT                   1      /**< Instance executing as a client */

#define WS_MAX_FRAME                131072 /**< Maximum frame size */
#define WS_MAX_MESSAGE              (1024 * 1024) /**< Maximum message size, zero for no limit */

/*
    webSendBlock message types
 */
#define WS_MSG_CONT                 0x0    /**< Continuation of WebSocket message */
#define WS_MSG_TEXT                 0x1    /**< webSendBlock type for text messages */
#define WS_MSG_BINARY               0x2    /**< webSendBlock type for binary messages */
#define WS_MSG_CONTROL              0x8    /**< Start of control messages */
#define WS_MSG_CLOSE                0x8    /**< webSendBlock type for close message */
#define WS_MSG_PING                 0x9    /**< webSendBlock type for ping messages */
#define WS_MSG_PONG                 0xA    /**< webSendBlock type for pong messages */
#define WS_MSG_MAX                  0xB    /**< Max message type for webSendBlock */
#define WS_MSG_MORE                 0x10   /**< Use on first call to webSendBlock to indicate more data to follow */

/*
    Close message status codes
    0-999       Unused
    1000-1999   Reserved for spec
    2000-2999   Reserved for extensions
    3000-3999   Library use
    4000-4999   Application use
 */
#define WS_STATUS_OK                1000   /**< Normal closure */
#define WS_STATUS_GOING_AWAY        1001   /**< Endpoint is going away. Server down or browser navigating away */
#define WS_STATUS_PROTOCOL_ERROR    1002   /**< WebSockets protocol error */
#define WS_STATUS_UNSUPPORTED_TYPE  1003   /**< Unsupported message data type */
#define WS_STATUS_FRAME_TOO_LARGE   1004   /**< Reserved. Message frame is too large */
#define WS_STATUS_NO_STATUS         1005   /**< No status was received from the peer in closing */
#define WS_STATUS_COMMS_ERROR       1006   /**< TCP/IP communications error  */
#define WS_STATUS_INVALID_UTF8      1007   /**< Text message has invalid UTF-8 */
#define WS_STATUS_POLICY_VIOLATION  1008   /**< Application level policy violation */
#define WS_STATUS_MESSAGE_TOO_LARGE 1009   /**< Message is too large */
#define WS_STATUS_MISSING_EXTENSION 1010   /**< Unsupported WebSockets extension */
#define WS_STATUS_INTERNAL_ERROR    1011   /**< Server terminating due to an internal error */
#define WS_STATUS_TLS_ERROR         1015   /**< TLS handshake error */
#define WS_STATUS_MAX               5000   /**< Maximum error status (less one) */

/*
    WebSocket states (rx->webSockState)
 */
#define WS_STATE_CONNECTING         0      /**< WebSocket connection is being established */
#define WS_STATE_OPEN               1      /**< WebSocket handsake is complete and ready for communications */
#define WS_STATE_CLOSING            2      /**< WebSocket is closing */
#define WS_STATE_CLOSED             3      /**< WebSocket is closed */

/**
    Allocate a new WebSocket object
    @description This routine allocates and initializes a new WebSocket object.
    @param sock Communication socket
    @param client True if the instance is a client, false if it is a server.
    @return The new WebSocket object
    @stability Evolving
 */
PUBLIC WebSocket *webSocketAlloc(RSocket *sock, bool client);

/**
    Free a WebSocket object
    @description This routine frees a WebSocket object allocated by #webSocketAlloc.
    @param ws WebSocket object
    @stability Evolving
 */
PUBLIC void webSocketFree(WebSocket *ws);

/**
    Process a packet of data containing webSocket frames and data
    @description This routine processes a WebSocket packet.
    @param ws WebSocket object
    @return 0 on close, < 0 for error and 1 for message(s) received
    @stability Evolving
 */
PUBLIC int webSocketProcess(WebSocket *ws);

/**
    Define the WebSocket callback
    @description This routine processes a WebSocket packet.
    @param ws WebSocket object
    @param callback Callback function
    @param arg User argument
    @param buf Buffer containting pre-read data that may have been received as part of reading the HTTP headers.
    @stability Evolving
 */
PUBLIC void webSocketAsync(WebSocket *ws, WebSocketProc callback, void *arg, RBuf *buf);

/**
    Wait for the WebSocket connection to close
    @param ws WebSocket object
    @param deadline Deadline for the operation
    @return 0 on close, < 0 for error and 1 for message(s) received
    @stability Evolving
 */
PUBLIC int webSocketWait(WebSocket *ws, Time deadline);

/**
    Get the client key
    @description The client key is a unique identifier for the client.
    @param ws WebSocket object
    @return The client key
    @stability Evolving
 */
PUBLIC cchar *webSocketGetClientKey(WebSocket *ws);

/**
    Get the close reason supplied by the peer.
    @description The peer may supply a UTF8 messages reason for the closure.
    @param ws WebSocket object
    @return The UTF8 reason string supplied by the peer when closing the WebSocket.
    @stability Evolving
 */
PUBLIC cchar *webSocketGetCloseReason(WebSocket *ws);

/**
    Get the WebSocket private data
    @description Get the private data defined with #webSocketSetData
    @param ws WebSocket object.
    @return The private data reference
    @stability Evolving
 */
PUBLIC void *webSocketGetData(WebSocket *ws);

/**
    Get the error message for the current message
    @description The error message will be set if an error occurs while parsing or processingthe message.
    @param ws WebSocket object
    @return The error message. Caller must not free the message.
    @stability Evolving
 */
PUBLIC cchar *webSocketGetErrorMessage(WebSocket *ws);

/**
    Get the message length for the current message
    @description The message length will be updated as the message frames are received.
    The message length is only complete when the last frame has been received.
    @param ws WebSocket object.
    @return The size of the message.
    @stability Evolving
 */
PUBLIC ssize webSocketGetMessageLength(WebSocket *ws);

/**
    Test if WebSocket connection was orderly closed by sending an acknowledged close message
    @param ws WebSocket object
    @return True if the WebSocket was orderly closed.
    @stability Evolving
 */
PUBLIC bool webSocketGetOrderlyClosed(WebSocket *ws);

/**
    Get the selected WebSocket protocol selected by the server
    @param ws WebSocket object.
    @return The WebSocket protocol string
    @stability Evolving
 */
PUBLIC char *webSocketGetProtocol(WebSocket *ws);

/**
    Get the WebSocket state
    @return The WebSocket state. Will be WS_STATE_CONNECTING, WS_STATE_OPEN, WS_STATE_CLOSING or WS_STATE_CLOSED.
    @stability Evolving
 */
PUBLIC ssize webSocketGetState(WebSocket *ws);

/**
    Send a UTF-8 text message to the WebSocket peer
    @description This call invokes webSocketSend with a type of WS_MSG_TEXT.
        The message must be valid UTF8 as the peer will reject invalid UTF8 messages.
    @param ws WebSocket object.
    @param fmt Printf style formatted string
    @param ... Arguments for the format
    @return Number of bytes written
    @stability Evolving
 */
PUBLIC ssize webSocketSend(WebSocket *ws, cchar *fmt, ...) PRINTF_ATTRIBUTE(2, 3);

/**
    Send a string to the WebSocket peer
    @description This call invokes webSocketSendBlock with a type of WS_MSG_TEXT.
    @param ws WebSocket object
    @param buf String to send
    @return Number of bytes written
    @stability Evolving
 */
PUBLIC ssize webSocketSendString(WebSocket *ws, cchar *buf);

/**
    Send a json object to the WebSocket peer
    @description This call invokes webSocketSendString with a type of WS_MSG_TEXT.
    @param ws WebSocket object
    @param json Json object
    @param nid Node id
    @param key Key
    @return Number of bytes written
    @stability Evolving
 */
PUBLIC ssize webSocketSendJson(WebSocket *ws, Json *json, int nid, cchar *key);

/**
    Flag for #webSocketSendBlock to indicate there are more frames for this message
 */
#define WEB_MORE 0x1000

/**
    Send a message of a given type to the WebSocket peer
    @description This API permits control of message types and message framing.
    \n\n
    This routine may block for up to the inactivity timeout if the outgoing socket is full. When blocked, other fibers
    will be allowed to run.
    \n\n
    This API may split the message into frames such that no frame is larger than the limit "webSocketsFrameSize".
    However, if the type has the more flag set by oring the WEB_MORE define to indicate there is more data to complete
    this entire message, the data provided to this call will not be split into frames and will not be aggregated
    with previous or subsequent messages.  i.e. frame boundaries will be presserved and sent as-is to the peer.
    \n\n

    @param ws WebSocket object.
    @param type Web socket message type. Choose from WS_MSG_TEXT, WS_MSG_BINARY or WS_MSG_PING.
        Do not send a WS_MSG_PONG message as it is generated internally by the Web Sockets module.
        Use webSocketSendClose to send a close message.
    @param msg Message data buffer to send
    @param len Length of msg
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        error code.
    @stability Evolving
 */
PUBLIC ssize webSocketSendBlock(WebSocket *ws, int type, cchar *msg, ssize len);

/**
    Send a close message to the WebSocket peer
    @description This call invokes webSocketSendBlock with a type of WS_MSG_CLOSE.
        The status and reason are encoded in the message. The reason is an optional UTF8 closure reason message.
    @param ws WebSocket object
    @param status Web socket status
    @param reason Optional UTF8 reason text message. The reason must be less than 124 bytes in length.
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        error code.
    @stability Evolving
 */
PUBLIC ssize webSocketSendClose(WebSocket *ws, int status, cchar *reason);

/**
    Set the client key
    @description Set the client key for the WebSocket object. This is used to identify the client.
    @param ws WebSocket object
    @param clientKey Client key
    @stability Evolving
 */
PUBLIC void webSocketSetClientKey(WebSocket *ws, cchar *clientKey);

/**
    Set the WebSocket private data
    @description Set private data to be retained by the garbage collector
    @param ws WebSocket object
    @param data Managed data reference.
    @stability Evolving
 */
PUBLIC void webSocketSetData(WebSocket *ws, void *data);

/**
    Set the WebSocket fiber
    @description Set the fiber for the WebSocket object.
    @param ws WebSocket object
    @param fiber Fiber
    @stability Evolving
 */
PUBLIC void webSocketSetFiber(WebSocket *ws, RFiber *fiber);

/**
    Set the ping period
    @description Set the ping period for the WebSocket object.
    @param ws WebSocket object
    @param pingPeriod Ping period
    @stability Evolving
 */
PUBLIC void webSocketSetPingPeriod(WebSocket *ws, Time pingPeriod);

/**
    Set the maximum frame size and message size
    @description Set the maximum frame size and message size for the WebSocket object.
    @param ws WebSocket object
    @param maxFrame Maximum frame size
    @param maxMessage Maximum message size
    @stability Evolving
 */
PUBLIC void webSocketSetLimits(WebSocket *ws, ssize maxFrame, ssize maxMessage);

/**
    Select the WebSocket protocol
    @description Select the WebSocket protocol for the WebSocket session.
    @param ws WebSocket object
    @param protocol WebSocket protocol
    @stability Evolving
 */
PUBLIC void webSocketSelectProtocol(WebSocket *ws, cchar *protocol);

/**
    Set whether to validate UTF8 codepoints
    @description Set whether to validate UTF8 codepoints for the WebSocket object.
    @param ws WebSocket object
    @param validateUTF True to validate UTF8 codepoints, false otherwise
    @stability Evolving
 */
PUBLIC void webSocketSetValidateUTF(WebSocket *ws, bool validateUTF);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_WEBSOCKETS */
#endif /* _h_WEBSOCKETS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
