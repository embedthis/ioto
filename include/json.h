/*
    json.h -- Header for the JSON library

    Supports JSON6 which is a strict JSON superset. Allows:

    . Keyword undefined
    . Multiline strings
    . Backtick quotes
    . Unquoted keys
    . Trailing commas in objects and arrays
    . Single and multiline comments

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#pragma once

#ifndef _h_JSON
#define _h_JSON 1

/********************************** Includes **********************************/

#include "r.h"

/*********************************** Defines **********************************/
#if ME_COM_JSON

#ifdef __cplusplus
extern "C" {
#endif

/******************************** JSON ****************************************/

struct Json;
struct JsonNode;

#ifndef JSON_BLEND
    #define JSON_BLEND  1
#endif

#ifndef ME_JSON_MAX_NODES
    #define ME_JSON_MAX_NODES 100000
#endif

/*
    Json types
 */
#define JSON_OBJECT     0x1
#define JSON_ARRAY      0x2
#define JSON_COMMENT    0x4
#define JSON_STRING     0x8                /**< Strings including dates encoded as ISO strings */
#define JSON_PRIMITIVE  0x10               /**< True, false, null, undefined, number */
#define JSON_REGEXP     0x20               /**< Regular expressions */

/*
    Constructor flags
 */
#define JSON_LOCK       0x1                /**< Lock JSON object from further object */
#define JSON_USER_ALLOC 0x2                /**< User flag to indicate who allocated the json obj */

/*
    Parse flags
 */
#define JSON_STRICT     0x10               /**< Expect strict JSON format. Otherwise allow relaxed Json6. */
#define JSON_SINGLE     0x20               /**< Save on a sinle line where possible. Convert chars to \\alternatives */
#define JSON_PASS_VALUE 0x40               /**< Transfer ownership of the parsed value to json. */

/*
    ToString flags
    Use JSON_PRETTY for a human-readable multiline format in json6.
    Use JSON_QUOTES for a strict JSON format with key double quotes and value double quotes.
    Use JSON_SINGLE for a single line format.
    Use JSON_STRICT for (JSON_QUOTES | JSON_SINGLE)
 */
#define JSON_PRETTY     0x100              /**< Save in multiline, indented Json6 format without key quotes */
#define JSON_QUOTES     0x200              /**< Save in strict JSON format with key quotes */
#define JSON_BARE       0x400              /**< Save on a single line without quotes or [], {} */
#define JSON_KEY        0x800              /* Internal flag to designate Property key */
#define JSON_DEBUG      0x1000             /* Internal flag for debug formatting */

/**
    These macros iterates over the children under the "parent" id. The child->last points to one past the end of the
    Property value and parent->last points to one past the end of the parent object.
    WARNING: this macro requires a stable JSON collection. I.e. the collection must not be modified in the loop body.
    Insertions and removals in prior nodes in the JSON tree will change the values pointed to by the child pointer and
    will impact further iterations.
    The jsonCheckIteration function will catch some (but not all) modifications to the JSON tree.
 */

/**
    Iterate over the children under the "parent" node.
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param parent The parent node
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON(json, parent, child, nid) \
        int pid = (int) ((parent ? parent : json->nodes) - json->nodes), nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    Iterate over the children under a node identified by its ID
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param pid The parent node ID
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON_ID(json, pid, child, nid) \
        int nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    Iterate over the children under a given key node
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param baseId The node from which to search for the key
    @param key The key to search for
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON_KEY(json, baseId, key, child, nid) \
        int parentId = jsonGetId(json, baseId, key), nid = parentId + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[parentId].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

//  DEPRECATED - use ITERATE_JSON_ID
#define ITERATE_JSON_DYNAMIC(json, pid, child, nid) \
        int nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    JSON Object
    @description The JSON library parses JSON strings into an in-memory JSON tree that can be
        queried and modified and saved. Some APIs such as jsonGetRef return direct references into
        the JSON tree for performance as (const char*) references. Others, such as jsonGet will
        return an allocated string that must be freed by the caller.
        \n
        When a JSON object is allocated or parsed, the JSON tree may be locked via the JSON_LOCK flag.
        A locked JSON object is useful as it will not permit further updates via (jsonSet or jsonBlend)
        and the internal node structure will be stable such that references returned via
        jsonGetRef and jsonGetNode will remain valid.
    @stability Evolving
    @see
 */
typedef struct Json {
    struct JsonNode *nodes;
#if R_USE_EVENT
    REvent event;                           /**< Saving event */
#endif
    char *text;                             /**< Text being parsed */
    char *end;                              /**< End of text + 1 */
    char *next;                             /**< Pointer to next token */
    char *path;                             /**< Filename being parsed */
    char *errorMsg;                         /**< Parsing error details */
    char *value;                            /**< Result from jsonString */
    char *property;                         /**< Current property buffer */
    ssize propertyLength;                   /**< Property buffer length */
    uint size;                              /**< Size of Json.nodes in elements (includes spare) */
    uint count;                             /**< Number of allocated nodes (count <= size) */
    uint lineNumber : 16;                   /**< Current parse line number */
    uint flags : 16;                        /**< Use defined bits */
    uint strict : 1;                        /**< Strict JSON standard mode */
#if JSON_TRIGGER
    JsonTrigger trigger;
    void *triggerArg;
#endif
} Json;

/**
    JSON Node
    @stability Evolving
 */
typedef struct JsonNode {
    char *name;                                 /**< Property name - null terminated */
    char *value;                                /**< Property value - null terminated */
    int last : 24;                              /**< Index +1 of last node for which this node is parent */
    uint type : 6;                              /**< Primitive type of the node (object, array, string or primitive) */
    uint allocatedName : 1;                     /**< True if the node text was allocated and must be freed */
    uint allocatedValue : 1;                    /**< True if the node text was allocated and must be freed */
#if ME_DEBUG
    int lineNumber;                             /**< Debug only - json line number */
#endif
} JsonNode;

#if JSON_TRIGGER
/**
    Trigger callback for
 */
typedef void (*JsonTrigger)(void *arg, struct Json *json, JsonNode *node, cchar *name, cchar *value, cchar *oldValue);
PUBLIC void jsonSetTrigger(Json *json, JsonTrigger proc, void *arg);
#endif

/**
    Allocate a json object
    @param flags Set to one JSON_STRICT for strict JSON parsing. Otherwise permits JSON/5.
    @return A json object
    @stability Evolving
 */
PUBLIC Json *jsonAlloc(int flags);

/**
    Free a json object
    @param json A json object
    @stability Evolving
 */
PUBLIC void jsonFree(Json *json);

/**
    Lock a json object from further updates
    @description This call is useful to block all further updates via jsonSet.
        The jsonGet API returns a references into the JSON tree. Subsequent updates can grow
        the internal JSON structures and thus move references returned earlier.
    @param json A json object
    @stability Evolving
 */
PUBLIC void jsonLock(Json *json);

/**
    Unlock a json object to allow updates
    @param json A json object
    @stability Evolving
 */
PUBLIC void jsonUnlock(Json *json);

/*
    Flags for jsonBlend
 */
#define JSON_COMBINE      0x1            /**< Combine properies using '+' '-' '=' '?' prefixes */
#define JSON_OVERWRITE    0x2            /**< Default to overwrite existing properies '=' */
#define JSON_APPEND       0x4            /**< Default to append to existing '+' (default) */
#define JSON_REPLACE      0x8            /**< Replace existing properies '-' */
#define JSON_CCREATE      0x10           /**< Conditional create if not already existing '?' */
#define JSON_REMOVE_UNDEF 0x20           /**< Remove undefined (NULL) properties */

/**
    Blend nodes by copying from one Json to another.
    @description This performs an N-level deep clone of the source JSON nodes to be blended into the destination object.
        By default, this add new object properies and overwrite arrays and string values.
        The Property combination prefixes: '+', '=', '-' and '?' to append, overwrite, replace and
            conditionally overwrite are supported if the JSON_COMBINE flag is present.
    @param dest Destination json
    @param did Base node ID from which to store the copied nodes.
    @param dkey Destination property name to search for.
    @param src Source json
    @param sid Base node ID from which to copy nodes.
    @param skey Source property name to search for.
    @param flags The JSON_COMBINE flag enables Property name prefixes: '+', '=', '-', '?' to append, overwrite,
        replace and conditionally overwrite key values if not already present. When adding string properies, values
        will be appended using a space separator. Extra spaces will not be removed on replacement.
            \n\n
        Without JSON_COMBINE or for properies without a prefix, the default is to blend objects by creating new
        properies if not already existing in the destination, and to treat overwrite arrays and strings.
        Use the JSON_OVERWRITE flag to override the default appending of objects and rather overwrite existing
        properies. Use the JSON_APPEND flag to override the default of overwriting arrays and strings and rather
        append to existing properies.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int jsonBlend(Json *dest, int did, cchar *dkey, const Json *src, int sid, cchar *skey, int flags);

/**
    Clone a json object
    @param src Input json object
    @param flags Reserved, set to zero.
    @return The copied JSON tree. Caller must free with #jsonFree.
    @stability Evolving
 */
PUBLIC Json *jsonClone(const Json *src, int flags);

/**
    Get a json node value as an allocated string
    @description This call returns an allocated string as the result. Use jsonGetRef as a higher
    performance API if you do not need to retain the queried value.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return a copy of the defaultValue. The defaultValue
    can be NULL in which case the return value will be an allocated empty string.
    @return An allocated string copy of the key value or defaultValue if not defined. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonGetClone(Json *json, int nid, cchar *key, cchar *defaultValue);

#if DEPRECATED || 1
/**
    Get a json node value as a string
    @description This call is DEPRECATED. Use jsonGet or jsonGetClone instead.
        This call returns a reference into the JSON storage. Such references are
        short-term and may not remain valid if other modifications are made to the JSON tree.
        Only use the result of this API while no other changes are made to the JSON object.
        Use jsonGet if you need to retain the queried value.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a string or defaultValue if not defined. This is a reference into the
        JSON store. Caller must NOT free.
    @stability Deprecated
 */
PUBLIC cchar *jsonGetRef(Json *json, int nid, cchar *key, cchar *defaultValue);
#endif

/**
    Get a json node value as a string
    @description This call returns a reference into the JSON storage. Such references are
        short-term and may not remain valid if other modifications are made to the JSON tree.
        Only use the result of this API while no other changes are made to the JSON object.
        Use jsonGet if you need to retain the queried value.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a string or defaultValue if not defined. This is a reference into the
        JSON store. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *jsonGet(Json *json, int nid, cchar *key, cchar *defaultValue);

/**
    Get a json node value as a boolean
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a boolean or defaultValue if not defined
    @stability Evolving
 */
PUBLIC bool jsonGetBool(Json *json, int nid, cchar *key, bool defaultValue);

/**
    Get a json node value as a double
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a double or defaultValue if not defined
    @stability Evolving
 */
PUBLIC double jsonGetDouble(Json *json, int nid, cchar *key, double defaultValue);

/**
    Get a json node value as an integer
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as an integer or defaultValue if not defined
    @stability Evolving
 */
PUBLIC int jsonGetInt(Json *json, int nid, cchar *key, int defaultValue);

/**
    Get a json node value as a 64-bit integer
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a 64-bit integer or defaultValue if not defined
    @stability Evolving
 */
PUBLIC int64 jsonGetNum(Json *json, int nid, cchar *key, int64 defaultValue);

/**
    Get a json node value as a uint64
    @description Parse the stored value with unit suffixes and returns a number. 
    The following suffixes are supported:
        sec, secs, second, seconds,
        min, mins, minute, minutes,
        hr, hrs, hour, hours,
        day, days,
        week, weeks,
        month, months,
        year, years,
        byte, bytes, k, kb, m, mb, g, gb.
        Also supports the strings: unlimited, infinite, never, forever.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a uint64 or defaultValue if not defined
    @stability Evolving
 */
PUBLIC uint64 jsonGetValue(Json *json, int nid, cchar *key, cchar *defaultValue);

/**
    Get a json node ID
    @param json Source json
    @param nid Base node ID from which to start the search. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The node ID for the specified key
    @stability Evolving
 */
PUBLIC int jsonGetId(const Json *json, int nid, cchar *key);

/**
    Get a json node object
    @description This call returns a reference into the JSON storage. Such references are
        not persistent if other modifications are made to the JSON tree.
    @param json Source json
    @param nid Base node ID from which to start the search. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The node object for the specified key. Returns NULL if not found.
    @stability Evolving
 */
PUBLIC JsonNode *jsonGetNode(const Json *json, int nid, cchar *key);

/*
    Get a json node object ID
    @description This call returns the node ID for a node. Such references are
        not persistent if other modifications are made to the JSON tree.
    @param json Source json
    @param node Node reference
    @return The node ID.
    @stability Evolving
 */
PUBLIC int jsonGetNodeId(const Json *json, JsonNode *node);

/**
    Get the Nth child node for a json node.
    @param json Source json
    @param nid Base node ID to examine.
    @param nth Specify which child to return.
    @return The Nth child node object for the specified node.
    @stability Evolving
 */
PUBLIC JsonNode *jsonGetChildNode(Json *json, int nid, int nth);

/**
    Get the value type for a node
    @param json Source json
    @param nid Base node ID from which to start the search.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The data type. Set to JSON_OBJECT, JSON_ARRAY, JSON_COMMENT, JSON_STRING, JSON_PRIMITIVE or JSON_REGEXP.
    @stability Evolving
 */
PUBLIC int jsonGetType(Json *json, int nid, cchar *key);

/**
    Parse a json string into a json object
    @description Use this method if you are sure the supplied JSON text is valid or do not need to receive
        diagnostics of parse failures other than the return value.
    @param text Json string to parse.
    @param flags Set to JSON_STRICT to parse json, otherwise a relaxed json6 syntax is supported.
    Set to JSON_LOCK to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGetRef and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC Json *jsonParse(cchar *text, int flags);

/**
    Parse a json string into a json object and assume ownership of the supplied text memory.
    @description This is an optimized version of jsonParse that avoids copying the text to be parsed.
    The ownership of the supplied text is transferred to the Json object and will be freed when 
    jsonFree is called. The caller must not free the text which will be freed by this function.
    Use this method if you are sure the supplied JSON text is valid or do not 
    need to receive diagnostics of parse failures other than the return value.
    @param text Json string to parse. Caller must NOT free.
    @param flags Set to JSON_STRICT to parse json, otherwise a relaxed json6 syntax is supported.
    Set to JSON_LOCK to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGetRef and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC Json *jsonParseKeep(char *text, int flags);

/**
    Convert a string into strict json
    @param fmt Printf style format string
    @param ... Args for format
    @return A string. Returns NULL if the text will not parse. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonConvert(cchar *fmt, ...);

#if DEPRECATED || 1
#define jsonFmtToString jsonConvert
#endif

/**
    Convert a string into strict json string in a buffer.
    @param fmt Printf style format string
    @param ... Args for format
    @param buf Destination buffer
    @param size Size of the destination buffer
    @return The reference to the buffer
    @stability Evolving
 */
PUBLIC cchar *jsonConvertBuf(char *buf, size_t size, cchar *fmt, ...);

/*
    Convenience macro for converting a format and string into a strict json string in a buffer.
 */
#define JFMT(buf, ...) jsonConvertBuf(buf, sizeof(buf), __VA_ARGS__)

/*
    Convenience macro for converting a string into a strict json string in a buffer.
 */
#define JSON(buf, ...) jsonConvertBuf(buf, sizeof(buf), "%s", __VA_ARGS__)

/**
    Parse a formatted string into a json object
    @param fmt Printf style format string
    @param ... Args for format
    @return A json object. Caller must free.
    @stability Evolving
 */
PUBLIC Json *jsonParseFmt(cchar *fmt, ...);

/**
    Load a JSON object from a filename
    @param path Filename path containing a JSON string to load
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_STRICT to parse json, otherwise a relaxed json6 syntax is supported.
    @return JSON object tree. Caller must free errorMsg via rFree on errors.
    @stability Evolving
 */
PUBLIC Json *jsonParseFile(cchar *path, char **errorMsg, int flags);

/**
    Parse a JSON string into an object tree and return any errors.
    @description Deserializes a JSON string created into an object.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param text JSON string to deserialize.
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_STRICT to parse json, otherwise a relaxed json6 syntax is supported. 
    @return Returns a tree of Json objects. Each object represents a level in the JSON input stream.
        Caller must free errorMsg via rFree on errors.
    @stability Evolving
 */
PUBLIC Json *jsonParseString(cchar *text, char **errorMsg, int flags);

/**
    Remove a Property from a JSON object
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to remove for. This may include ".". For example: "settings.mode".
    @return Returns a JSON object array of all removed properies. Array will be empty if not qualifying
        properies were found and removed.
    @stability Evolving
 */
PUBLIC int jsonRemove(Json *obj, int nid, cchar *key);

/**
    Save a JSON object to a filename
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param path Filename path to contain the saved JSON string
    @param flags Same flags as for #jsonToString: JSON_PRETTY, JSON_QUOTES.
    @param mode Permissions mode
    @return Zero if successful, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int jsonSave(Json *obj, int nid, cchar *key, cchar *path, int mode, int flags);

/**
    Update a key/value in the JSON object with a string value
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Character string value.
    @param type Set to JSON_ARRAY, JSON_OBJECT, JSON_PRIMITIVE or JSON_STRING.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSet(Json *obj, int nid, cchar *key, cchar *value, int type);

/**
    Update a key in the JSON object with a JSON object value passed as a JSON5 string
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param fmt JSON string.
    @param ... Args for format
    @return Zero if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetJsonFmt(Json *json, int nid, cchar *key, cchar *fmt, ...);

/**
    Update a property in the JSON object with a boolean value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse.
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Boolean string value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetBool(Json *obj, int nid, cchar *key, bool value);

/**
    Update a property with a floating point number value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Double floating point value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetDouble(Json *json, int nid, cchar *key, double value);

/**
    Update a property in the JSON object with date value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Date value expressed as a Time (Elapsed milliseconds since Jan 1, 1970).
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetDate(Json *json, int nid, cchar *key, Time value);

/**
    Update a key/value in the JSON object with a formatted string value
    @description The type of the inserted value is determined from the contents.
    This call takes a multipart property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param fmt Printf style format string
    @param ... Args for format
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetFmt(Json *obj, int nid, cchar *key, cchar *fmt, ...);

/**
    Update a property in the JSON object with a numeric value
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Number to update.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetNumber(Json *json, int nid, cchar *key, int64 value);

/**
    Update a property in the JSON object with a string value.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value String value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetString(Json *json, int nid, cchar *key, cchar *value);

/**
    Directly update a node value.
    @description This is an internal API and is subject to change without notice. It offers a higher performance path
        to update node values.
    @param node Json node
    @param value String value to update with.
    @param type Json node type
    @param flags Set to JSON_PASS_VALUE to transfer ownership of a string. JSON will then free.
    @stability Internal
 */
PUBLIC void jsonSetNodeValue(JsonNode *node, cchar *value, int type, int flags);

/**
    Update a node type.
    @description This is an internal API and is subject to change without notice. It offers a higher performance path
        to update node types.
    @param node Json node
    @param type Json node type
    @stability Internal
 */
PUBLIC void jsonSetNodeType(JsonNode *node, int type);

/**
    Convert a json value to serialized JSON representation and save in the given buffer.
    @param buf Destination buffer
    @param value Value to convert.
    @param flags Json flags.
    @stability Evolving
 */
PUBLIC void jsonToBuf(RBuf *buf, cchar *value, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
    @param json Source json
    @param nid Base node ID from which to convert. Set to zero for the top level.
    @param key Property name to serialize below. This may include ".". For example: "settings.mode".
    @param flags Serialization flags. Supported flags include JSON_PRETTY for a human-readable multiline format.
    Use JSON_STRICT for a strict JSON format. Use JSON_QUOTES to wrap property names in quotes.
    Defaults to JSON_PRETTY if set to zero.
    @return Returns a serialized JSON character string. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonToString(const Json *json, int nid, cchar *key, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
        This serialize the result into the json->value so the caller does not need to free the result.
    @param json Source json
    @param flags Serialization flags. Supported flags include JSON_PRETTY for a human-readable multiline format.
    Use JSON_STRICT for a strict JSON format. Use JSON_QUOTES to wrap property names in quotes.
    Defaults to JSON_PRETTY if set to zero.
    @return Returns a serialized JSON character string. Caller must NOT free. The string is owned by the json
    object and will be overwritten by subsequent calls to jsonString. It will be freed when jsonFree is called.
    @stability Evolving
 */
PUBLIC cchar *jsonString(const Json *json, int flags);

/**
    Print a JSON object
    @description Prints a JSON object in pretty format.
    @param json Source json
    @stability Evolving
 */
PUBLIC void jsonPrint(Json *json);

/**
    Expand a string template with ${prop.prop...} references
    @description Unexpanded references are replaced with $^{token}
    @param json Json object
    @param str String template to expand
    @param keep If true, unexpanded references are retained as ${token}
    @return An allocated expanded string. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonTemplate(Json *json, cchar *str, bool keep);

/**
    Check if the iteration is valid
    @param json Json object
    @param count Prior json count of nodes
    @param nid Node id
    @return Node id if valid, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonCheckIteration(Json *json, int count, int nid);
#ifdef __cplusplus
}
#endif

#endif /* ME_COM_JSON */
#endif /* _h_JSON */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
