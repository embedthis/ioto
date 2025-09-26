/**
    JSON5/JSON6 Parser and Manipulation Library
    @description High-performance JSON parser and manipulation library for embedded IoT applications.
    Supports both traditional JSON and relaxed JSON5/JSON6 syntax with extended features for ease of use.

    This library provides a complete JSON processing solution including:
    - Fast parsing of JSON/JSON5/JSON6 text into navigable tree structures
    - Insitu parsing of JSON text resulting in extremely efficient memory usage
    - Query API with dot-notation path support (e.g., "config.network.timeout")
    - Modification APIs for setting values and blending JSON objects
    - Serialization back to JSON text with multiple formatting options
    - Template expansion with ${path.var} variable substitution

    JSON5/JSON6 Extended Features:
    - Unquoted object keys when they contain no special characters
    - Unquoted string values when they contain no spaces
    - Trailing commas in objects and arrays
    - Single-line (//) and multi-line comments
    - Multi-line strings using backtick quotes
    - JavaScript-style primitives (undefined, null)
    - Keyword 'undefined' support

    The library is designed for embedded developers who need efficient JSON processing
    with minimal memory overhead and high performance characteristics.

    The parser is lax and will tolerate some non-standard JSON syntax such as:
    - Multiple or trailing commas in objects and arrays.
    - An empty string is allowed and returns an empty JSON instance.
    - Similarly a top level whitespace string is allowed and returns an empty JSON instance.

    Use another tool if you need strict JSON validation.

    @stability Evolving
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
    #define JSON_BLEND           1
#endif

#ifndef ME_JSON_MAX_NODES
    #define ME_JSON_MAX_NODES    100000               /**< Maximum number of elements in json text */
#endif

#ifndef JSON_MAX_LINE_LENGTH
    #define JSON_MAX_LINE_LENGTH 120                  /**< Default Maximum length of a line for compacted output */
#endif

#ifndef JSON_DEFAULT_INDENT
    #define JSON_DEFAULT_INDENT  4                    /**< Default indent level for json text */
#endif

/**
    JSON node type constants
    @description These constants define the different types of nodes in the JSON tree.
    Each node has exactly one type that determines how its value should be interpreted.
    @stability Evolving
 */
#define JSON_OBJECT              0x1                  /**< Object node containing key-value pairs */
#define JSON_ARRAY               0x2                  /**< Array node containing indexed elements */
#define JSON_COMMENT             0x4                  /**< Comment node (JSON5 feature) */
#define JSON_STRING              0x8                  /**< String value including ISO date strings */
#define JSON_PRIMITIVE           0x10                 /**< Primitive values: true, false, null, undefined, numbers */
#define JSON_REGEXP              0x20                 /**< Regular expression literal (JSON6 feature) */

/**
    JSON parsing flags
    @description Flags that control the behavior of JSON parsing operations.
    These can be combined using bitwise OR to enable multiple options.
    @stability Evolving
 */
#define JSON_STRICT_PARSE        0x1                  /**< Parse in strict JSON format (no JSON5 extensions) */
#define JSON_PASS_VALUE          0x2                  /**< Transfer string ownership to JSON object during parsing */

/**
    JSON rendering flags
    @description Flags that control the format and style of JSON serialization output.
    These can be combined to achieve the desired output format.
    @stability Evolving
 */
#define JSON_COMPACT             0x10                 /**< Use compact formatting with minimal whitespace */
#define JSON_DOUBLE_QUOTES       0x20                 /**< Use double quotes for strings and keys */
#define JSON_ENCODE              0x40                 /**< Encode control characters in strings */
#define JSON_EXPAND              0x80                 /**< Expand ${path.var} template references during rendering */
#define JSON_MULTILINE           0x100                /**< Format output across multiple lines for readability */
#define JSON_ONE_LINE            0x200                /**< Force all output onto a single line */
#define JSON_QUOTE_KEYS          0x400                /**< Always quote object property keys */
#define JSON_SINGLE_QUOTES       0x800                /**< Use single quotes instead of double quotes */

/**
    Internal rendering flags
    @description Internal flags used by the JSON library during rendering operations.
    These are not intended for direct use by applications.
    @stability Internal
 */
#define JSON_KEY                 0x1000               /**< Internal: currently rendering a property key */
#define JSON_DEBUG               0x2000               /**< Internal: enable debug-specific formatting */
#define JSON_BARE                0x4000               /**< Internal: render without quotes or brackets */

/**
    Internal parsing flags
    @description Internal flags used by the JSON library during parsing operations.
    These are not intended for direct use by applications.
    @stability Internal
 */
#define JSON_EXPANDING           0x8000               /**< Internal: expanding a ${path.var} reference */
#define JSON_EXPECT_KEY          0x10000              /**< Internal: parsing and expect a property key name */
#define JSON_EXPECT_COMMA        0x20000              /**< Internal: parsing and expect a comma */
#define JSON_EXPECT_VALUE        0x40000              /**< Internal: parsing and expect a value */
#define JSON_PARSE_FLAGS         0xFF000              /**< Internal: parsing flags */

/**
    Composite formatting flags
    @description Predefined combinations of formatting flags for common output styles.
    These provide convenient presets for typical use cases.
    @stability Evolving
 */
#define JSON_JS                  (JSON_SINGLE_QUOTES) /**< JavaScript-compatible format with single quotes */
/**<
    Strict JSON format compliant with RFC 7159
 */
#define JSON_JSON                (JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS | JSON_ENCODE)
#define JSON_JSON5               (JSON_SINGLE_QUOTES) /**< JSON5 format allowing relaxed syntax */
/**<
    Human-readable format with proper indentation
 */
#define JSON_HUMAN               (JSON_JSON5 | JSON_MULTILINE | JSON_COMPACT)

// DEPRECATED - this will be removed in the next release
#define JSON_PRETTY              (JSON_HUMAN)
#define JSON_QUOTES              (JSON_DOUBLE_QUOTES)
#define JSON_STRICT              (JSON_STRICT_PARSE | JSON_JSON)

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
    @description The primary JSON container structure that holds a parsed JSON tree in memory.
    This structure provides efficient access to JSON data through a node-based tree representation.

    The JSON library parses JSON text into an in-memory tree that can be queried, modified,
    and serialized back to text. APIs like jsonGet() return direct references into the tree
    for performance, while APIs like jsonGetClone() return allocated copies that must be freed.

    The JSON tree can be locked via jsonLock() to prevent modifications. A locked JSON object
    ensures that references returned by jsonGet() and jsonGetNode() remain valid, making it
    safe to hold multiple references without concern for tree modifications.

    Memory management is handled automatically through the R runtime. The entire tree is freed
    when jsonFree() is called on the root JSON object.
    @stability Evolving
 */
typedef struct Json {
    struct JsonNode *nodes;              /**< Array of JSON nodes forming the tree structure */
#if R_USE_EVENT
    REvent event;                        /**< Event structure for asynchronous saving operations */
#endif
    char *text;                          /**< Original JSON text being parsed (will be modified during parsing) */
char *end;                           /**< Pointer to one byte past the end of the text buffer */
char *next;                          /**< Current parsing position in the text buffer */
char *path;                          /**< File path if JSON was loaded from a file (for error reporting) */
char *error;                         /**< Detailed error message from parsing failures */
char *property;                      /**< Internal buffer for building property names during parsing */
ssize propertyLength;                /**< Current allocated size of the property buffer */
    char *value;                         /**< Cached serialized string result from jsonString() calls */
    uint size;                           /**< Total allocated capacity of the nodes array */
    uint count;                          /**< Number of nodes currently used in the tree */
    uint lineNumber : 16;                /**< Current line number during parsing (for error reporting) */
    uint lock : 1;                       /**< Lock flag preventing modifications when set */
    uint flags : 7;                      /**< Internal parser flags (reserved for library use) */
    uint userFlags : 8;                  /**< Application-specific flags available for user use */
#if JSON_TRIGGER
    JsonTrigger trigger;                 /**< Optional callback function for monitoring changes */
    void *triggerArg;                    /**< User argument passed to the trigger callback */
#endif
} Json;

/**
    JSON Node
    @description Individual node in the JSON tree representing a single property or value.
    Each node contains a name/value pair and maintains structural information about its
    position in the tree hierarchy.

    The JSON tree is stored as a flattened array of nodes with parent-child relationships
    maintained through indexing. The 'last' field indicates the boundary of child nodes,
    enabling efficient tree traversal without requiring explicit pointers.

    Memory management for name and value strings is tracked through the allocatedName
    and allocatedValue flags, allowing the library to optimize memory usage by avoiding
    unnecessary string copies when possible.
    @stability Evolving
 */
typedef struct JsonNode {
    char *name;                /**< Property name (null-terminated string, NULL for array elements) */
    char *value;               /**< Property value (null-terminated string representation) */
    int last : 24;             /**< Index + 1 of the last descendant node (defines subtree boundary) */
    uint type : 6;             /**< Node type: JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE, etc. */
    uint allocatedName : 1;    /**< True if name string was allocated and must be freed by JSON library */
    uint allocatedValue : 1;   /**< True if value string was allocated and must be freed by JSON library */
#if ME_DEBUG
    int lineNumber;            /**< Source line number in original JSON text (debug builds only) */
#endif
} JsonNode;

#if JSON_TRIGGER
/**
    Trigger callback for monitoring JSON modifications
    @description Callback function signature for receiving notifications when JSON nodes are modified.
    The trigger is called whenever a node value is changed through jsonSet or jsonBlend operations.
    @param arg User-defined argument passed to jsonSetTrigger()
    @param json The JSON object being modified
    @param node The specific node that was modified
    @param name The property name that was modified
    @param value The new value assigned to the property
    @param oldValue The previous value before modification
    @stability Evolving
 */
typedef void (*JsonTrigger)(void *arg, struct Json *json, JsonNode *node, cchar *name, cchar *value, cchar *oldValue);
PUBLIC void jsonSetTrigger(Json *json, JsonTrigger proc, void *arg);
#endif

/**
    Allocate a new JSON object
    @description Creates a new, empty JSON object ready for parsing or manual construction.
    The object is allocated using the R runtime and must be freed with jsonFree() when no longer needed.
    The initial object contains no nodes and is ready to accept JSON text via jsonParseText() or
    manual node construction via jsonSet() calls.
    @return A newly allocated JSON object, or NULL if allocation fails
    @stability Evolving
 */
PUBLIC Json *jsonAlloc(void);

/**
    Free a JSON object and all associated memory
    @description Releases all memory associated with a JSON object including the node tree,
    text buffers, and any allocated strings. After calling this function, the JSON object
    and all references into it become invalid and must not be used.
    @param json JSON object to free. Can be NULL (no operation performed).
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

/**
    Set user-defined flags on a JSON object
    @description Sets application-specific flags in the userFlags field of the JSON object.
    These flags are reserved for user applications and are not used by the JSON library.
    Useful for tracking application state or marking JSON objects with custom attributes.
    @param json JSON object to modify
    @param flags User-defined flags (8-bit value)
    @stability Evolving
 */
PUBLIC void jsonSetUserFlags(Json *json, int flags);

/**
    Get user-defined flags from a JSON object
    @description Retrieves the current value of application-specific flags from the JSON object.
    These flags are managed entirely by the user application.
    @param json JSON object to query
    @return Current user flags value (8-bit value)
    @stability Evolving
 */
PUBLIC int jsonGetUserFlags(Json *json);

/**
    JSON blending operation flags
    @description Flags that control how jsonBlend() merges JSON objects together.
    These can be combined to achieve sophisticated merging behaviors.
    @stability Evolving
 */
#define JSON_COMBINE      0x1            /**< Enable property name prefixes '+', '-', '=', '?' for merge control */
#define JSON_OVERWRITE    0x2            /**< Default behavior: overwrite existing properties (equivalent to '=') */
#define JSON_APPEND       0x4            /**< Default behavior: append to existing properties (equivalent to '+') */
#define JSON_REPLACE      0x8            /**< Default behavior: replace existing properties (equivalent to '-') */
#define JSON_CCREATE      0x10           /**< Default behavior: conditional create only if not existing (equivalent to
                                            '?') */
#define JSON_REMOVE_UNDEF 0x20           /**< Remove properties with undefined (NULL) values during blend */

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
    @description This call returns an allocated string as the result. Use jsonGet as a higher
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
        Use jsonGet if you need to retain the queried value. If a key value is NULL or undefined,
        then the defaultValue will be returned.
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
    Get a json node value as a date
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a date or defaultValue if not defined
    @stability Evolving
 */
PUBLIC Time jsonGetDate(Json *json, int nid, cchar *key, int64 defaultValue);

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

/**
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
    Get the Nth child node for a json node
    @description Retrieves a specific child node by its index position within a parent node.
    This is useful for iterating through array elements or object properties in order.
    The child index is zero-based.
    @param json Source json
    @param nid Base node ID to examine.
    @param nth Zero-based index of which child to return.
    @return The Nth child node object for the specified node. Returns NULL if the index is out of range.
    @stability Evolving
 */
PUBLIC JsonNode *jsonGetChildNode(Json *json, int nid, int nth);

/**
    Get the value type for a node
    @description Determines the type of a JSON node, which indicates how the value should be interpreted.
    This is essential for type-safe access to JSON values.
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
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
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
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC Json *jsonParseKeep(char *text, int flags);

/**
    Parse a json string into an existing json object
    @description Use this method if you need to have access to the error message if the parse fails.
    @param json Existing json object to parse into.
    @param text Json string to parse.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC int jsonParseText(Json *json, char *text, int flags);

/**
    Parse a string as JSON or JSON5 and convert into strict JSON string.
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
    Convenience macro for converting a JSON5 string into a strict JSON string in a buffer.
 */
#define JSON(buf, ...) jsonConvertBuf(buf, sizeof(buf), "%s", __VA_ARGS__)

/**
    Parse a formatted string into a json object
    @description Convenience function that formats a printf-style string and then parses it as JSON.
    This is useful for constructing JSON from dynamic values without manual string building.
    @param fmt Printf style format string
    @param ... Args for format
    @return A json object. Caller must free via jsonFree().
    @stability Evolving
 */
PUBLIC Json *jsonParseFmt(cchar *fmt, ...);

/**
    Load a JSON object from a filename
    @description Reads and parses a JSON file from disk into a JSON object tree.
    This is a convenience function that combines file reading with JSON parsing.
    If parsing fails, detailed error information is provided.
    @param path Filename path containing a JSON string to load
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    @return JSON object tree. Caller must free via jsonFree(). Returns NULL on error.
    @stability Evolving
 */
PUBLIC Json *jsonParseFile(cchar *path, char **errorMsg, int flags);

/**
    Parse a JSON string into an object tree and return any errors.
    @description Deserializes a JSON string created into an object.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param text JSON string to deserialize.
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    @return Returns a tree of Json objects. Each object represents a level in the JSON input stream.
        Caller must free errorMsg via rFree on errors.
    @stability Evolving
 */
PUBLIC Json *jsonParseString(cchar *text, char **errorMsg, int flags);

/**
    Remove a Property from a JSON object
    @description Removes one or more properties from a JSON object based on the specified key path.
    The key path supports dot notation for nested property removal. This operation modifies
    the JSON tree in place.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to remove. This may include ".". For example: "settings.mode".
    @return Returns zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonRemove(Json *obj, int nid, cchar *key);

/**
    Save a JSON object to a filename
    @description Serializes a JSON object (or a portion of it) to a file on disk.
    The output format is controlled by the flags parameter. The file is created with
    the specified permissions mode.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to save. Set to NULL to save the entire object. This may include ".". For example:
       "settings.mode".
    @param path Filename path to contain the saved JSON string
    @param mode File permissions mode (e.g., 0644)
    @param flags Rendering flags - same as for jsonToString()
    @return Zero if successful, otherwise a negative error code.
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
    Convert a string value primitive to a JSON string and add to the given buffer.
    @param buf Destination buffer
    @param value String value to convert
    @param flags Json flags
    @stability Evolving
 */
PUBLIC void jsonPutValueToBuf(RBuf *buf, cchar *value, int flags);

/**
    Convert a json object to a serialized JSON representation in the given buffer.
    @param buf Destination buffer
    @param json Json object
    @param nid Base node ID from which to convert. Set to zero for the top level.
    @param flags Json flags.
    @stability Evolving
 */
PUBLIC int jsonPutToBuf(RBuf *buf, const Json *json, int nid, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
    @param json Source json
    @param nid Base node ID from which to convert. Set to zero for the top level.
    @param key Property name to serialize below. This may include ".". For example: "settings.mode".
    @param flags Serialization flags. Supported flags include JSON_JSON5 and JSON_HUMAN.
    Use JSON_JSON for a strict JSON format. Defaults to JSON_HUMAN if set to zero.
    @return Returns a serialized JSON character string. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonToString(const Json *json, int nid, cchar *key, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
        This serialize the result into the json->value so the caller does not need to free the result.
    @param json Source json
    @param flags Serialization flags. Supported flags include JSON_HUMAN for a human-readable multiline format.
    Use JSON_JSON for a strict JSON format. Use JSON_QUOTES to wrap property names in quotes.
    Defaults to JSON_HUMAN if set to zero.
    @return Returns a serialized JSON character string. Caller must NOT free. The string is owned by the json
    object and will be overwritten by subsequent calls to jsonString. It will be freed when jsonFree is called.
    @stability Evolving
 */
PUBLIC cchar *jsonString(const Json *json, int flags);

/**
    Print a JSON object
    @description Prints a JSON object in a compact human readable format.
    @param json Source json
    @stability Evolving
 */
PUBLIC void jsonPrint(Json *json);

/**
    Expand a string template with ${prop.prop...} references
    @description Unexpanded references left as is.
    @param json Json object
    @param str String template to expand
    @param keep If true, unexpanded references are retained as ${token}, otherwise removed.
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

/**
    Set the maximum length of a line for compacted output.
    @param length Maximum length of a line for compacted output.
    @stability Evolving
 */
PUBLIC void jsonSetMaxLength(int length);

/**
    Set the indent level for compacted output.
    @param indent Indent level for compacted output.
    @stability Evolving
 */
PUBLIC void jsonSetIndent(int indent);

/**
    Get the length of a property value.
    If an array, return the array length. If an object, return the number of object properties.
    @param json Json object
    @param nid Node id
    @param key Property name
    @return Length of the property value, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC ssize jsonGetLength(Json *json, int nid, cchar *key);

/**
    Get the error message from the JSON object
    @param json Json object
    @return The error message. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *jsonGetError(Json *json);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_JSON */
#endif /* _h_JSON */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
