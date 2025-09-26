/** 
   @file db.h
   Embedded NoSQL Document Database
   @description High-performance embedded NoSQL document database for ANSI C applications.
   Provides JSON5/JSON6 document storage with red/black tree indexing for efficient queries.
   Features include transaction journaling with crash recovery, schema validation and enforcement,
   time-based item expiration, result pagination for large datasets, and optional cloud
   synchronization via triggers. Designed for embedded IoT applications requiring fast,
   reliable local data storage with minimal memory footprint.
   @stability Evolving

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#pragma once

#ifndef _h_DB
#define _h_DB       1

/********************************** Includes **********************************/

#define ME_COM_RB   1
#define ME_COM_JSON 1

#include "json.h"

/*********************************** Defines **********************************/
#if ME_COM_DB


#ifdef __cplusplus
extern "C" {
#endif

struct Db;
struct DbItem;
struct DbModel;
struct DbParams;

#define DB_VERSION          1

#ifndef DB_MAX_LOG_AGE
    #define DB_MAX_LOG_AGE  (60 * TPS)                  /**< Maximum age of log file */
#endif
#ifndef DB_MAX_LOG_SIZE
    #define DB_MAX_LOG_SIZE (1024 * 1024)               /**< Maximum journal size */
#endif
#ifndef DB_MAX_KEY
    #define DB_MAX_KEY      1024                        /**< Maximum sort key length */
#endif
#ifndef DB_MAX_ITEM
    #define DB_MAX_ITEM     (256 * 1024)                /**< Maximum database item length */
#endif

#define DB_ON_CHANGE        0x1
#define DB_ON_COMMIT        0x2
#define DB_ON_FREE          0x4

/**
    Database change notification callback
    @description Called when database items are modified, allowing applications to respond to data changes.
    Callbacks can be registered for specific events and models to enable features like cloud synchronization,
    audit logging, or cache invalidation.
    @param arg User-defined argument provided when registering the callback
    @param db Database instance where the change occurred
    @param model Schema model describing the item's structure and validation rules
    @param item Database item that was modified
    @param params API parameters used in the operation that triggered the change
    @param cmd Type of change operation: "create", "update", or "remove"
    @param events Event types that triggered this callback: DB_ON_CHANGE, DB_ON_COMMIT, or both
    @stability Evolving
 */
typedef void (*DbCallbackProc)(void *arg, struct Db *db, struct DbModel *model, struct DbItem *item,
                               struct DbParams *params, cchar *cmd, int events);

/**
    Main database instance structure
    @description High-performance NoSQL embedded database inspired by DynamoDB design principles.
    Stores JSON documents with schema-based validation and efficient red/black tree indexing.
    Provides transaction journaling for crash recovery, configurable persistence policies,
    and optional cloud synchronization capabilities. Designed for embedded applications requiring
    fast local data access with minimal memory footprint.
    @stability Evolving
 */
typedef struct Db {
    Json *schema;           /**< Database schema defining models, fields, and validation rules */
    char *path;             /**< On-disk path */
    RHash *models;          /**< List of schema models */
    RbTree *primary;        /**< Red/black tree primary index */
    RList *callbacks;       /**< Database change notification triggers */
    Json *context;          /**< Global context properties applied to all API operations */
    char *error;            /**< API error message */
    cchar *type;            /**< Name of the field used to identify item types in the schema */
    FILE *journal;          /**< Journal file descriptor */
    int flags;              /**< Database configuration flags (reserved for future use) */
    char *journalPath;      /**< On-disk journal filename */
    ssize journalSize;      /**< Current size of journal file */
    Ticks journalCreated;   /**< When journal file recreated */
    ssize maxJournalSize;   /**< Maximum size of the journal before saving */
    Ticks maxJournalAge;    /**< Maximum age of journal file before saving */
    REvent journalEvent;    /**< Timeout for journal save */
    REvent commitEvent;     /**< Timeout for commit event */
    RHash *changes;         /**< Hash of pending changes */
    Ticks due;              /**< When delayed commits are due */
    int code;               /**< API error code */
    bool journalError : 1;  /**< Journal I/O error */
    bool timestamps : 1;    /**< Maintain created/updated timestamps (if in schema) */
    bool servicing : 1;     /**< Servicing database */
    bool needSave : 1;      /**< Database needs saving */
} Db;

/*
    OneTable model field schema
    @internal
 */
typedef struct DbField {
    char *name;           /**< Field name */
    cchar *generate;      /**< Generate unique ID or ULID */
    uint ttl : 1;         /*<< Field is a TTL expiry field */
    uint hidden : 1;      /**< The field is hidden normally (pk, sk, etc) */
    uint required : 1;    /**< The field is required on create */
    cchar *def;           /**< Default value */
    cchar *value;         /**< Value template */
    cchar *type;          /**< Expected data type */
    char *enums;          /**< Set of enumerated valid values for the field */
} DbField;

/**
    Database model schema definition
    @description Defines an application entity type with its supported fields, validation rules,
    and behavior. Models provide schema validation, field type checking, and control persistence
    and synchronization policies for groups of related items.
    @stability Evolving
 */
typedef struct DbModel {
    cchar *name;            /**< Name of the model */
    uint sync : 1;          /**< Sync model items to the cloud */
    uint mem : 1;           /**< Keep model in-memory and not persisted to storage */
    cchar *expiresField;    /**< Name of the TTL field */
    Time delay;             /**< Time to delay before committing changes */
    RHash *fields;          /**< Hash of model fields */
} DbModel;

typedef int (*DbWhere)(Json *json, int nid, cvoid *arg);

/**
    Database parameters
    @stability Evolving
 */
typedef struct DbParams {
    bool bypass : 1;         /**< Bypass changes */
    bool log : 1;            /**< Emit trace information to the log */
    bool mem : 1;            /**< Update in memory only */
    bool upsert : 1;         /**< Update on create if present. Create on update if missing */
    int delay;               /**< Delay before commmiting changes (Delay in msec, -1 == nocommit) */
    int limit;               /**< Limit the number of returned or removed items */
    cchar *index;            /**< Index name. Default to "primary". Currently only supports
                                "primary" */
    cchar *next;             /**< Pagination token starting point for the next page of results */
    DbWhere where;           /**< Where query expression callback */
    cvoid *arg;              /**< Argument to where callback */
} DbParams;

#define DB_INMEM   -2        /**< Don't persist change to storage - preserve in memory only */
#define DB_NODELAY -1        /**< Don't delay, persist immediately */

/**
    Database items stored in RB indexes
    @stability Evolving
 */
typedef struct DbItem {
    char *key;               /**< Indexed name of the item. Used as the sort key */
    char *value;             /**< Text value of the item (JSON string), may be stale if json set */
    Json *json;              /**< Parsed JSON value of the item, takes precedence over value */
    uint allocatedName : 1;  /**< The name is allocated and must be freed when removed */
    uint allocatedValue : 1; /**< The value is allocated and must be freed when removed */
    uint delayed : 1;        /**< Update to journal and cloud delayed */
} DbItem;

typedef const DbItem CDbItem;

/**
    Macro for supplying API parameters
    @stability Evolving
 */
#define DB_PARAMS(...) ((DbParams[]){ { __VA_ARGS__ } })

/**
    Macro for supplying API properties as key/value pairs.
    @stability Evolving
 */
#define DB_PROPS(...)  dbPropsToJson((cchar*[]) { __VA_ARGS__, NULL })

/**
    Macro for supplying API properties as a string in JSON format.
    @description The parameters are a printf style format string with arguments.
    The allocated Json object has JSON_USER_ALLOC set so the database knows to free it when done.
    @stability Evolving
 */
#define DB_JSON(...)   dbStringToJson(__VA_ARGS__)

/**
    Add global context properties
    @description A global context property can be added to the set of properties supplied to each
       API.
        These property values are added at the top level only. Dotted notation is not supported.
    @param db Database instance
    @param name Name of the property
    @param value Property value to set.
    @stability Evolving
 */
PUBLIC void dbAddContext(Db *db, cchar *name, cchar *value);

/**
    Close a database
    @description This will immediately save any unsaved changes and then close the database.
    @param db Database instance returned via #dbOpen
    @stability Evolving
 */
PUBLIC void dbClose(Db *db);

/**
    Create a new item
    @description Create a new item of the required model type.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model entity to create. The model determines the valid set of
       properties for
        the created item. Set to NULL if no model is required.
    @param props JSON object containing the item properties to create. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        bool upsert : 1;      // Update on create if present. Create on update if missing.
        \n
    @return The created database item. The DbItem "name" property is the indexed key value. The
       value "json" property
        contains the item values as a cached JSON object. Caller must not free the returned item.
        Returns null on errors. Use dbGetError to retrieve an error message.
    @stability Evolving
 */
PUBLIC const DbItem *dbCreate(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Fetch a field value from an item as a string.
    @description Use to examine an item returned via dbGet or other APIs that return items.
    @note: WARNING: This function returns a pointer to a string that is part of the DbItem structure.
      Subsequent updates to the item will invalidate the pointer.
    @param item Database item returned from other APIs.
    @param fieldName Name of the field to examine.
    @return The field value as a string. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *dbField(const DbItem *item, cchar *fieldName);

/**
    Fetch a field value from an item as a number (64 bit).
    @description Use to examine an item returned via dbGet or other APIs that return items.
    @param item Database item returned from other APIs.
    @param fieldName Name of the field to examine.
    @return The field value as a 64 bit integer.
    @stability Evolving
 */
PUBLIC int64 dbFieldNumber(const DbItem *item, cchar *fieldName);

/**
    Fetch a field value from an item as a boolean
    @description Use to examine an item returned via dbGet or other APIs that return items.
        Will return true if the item field value is set to "true" or "1".
    @param item Database item returned from other APIs.
    @param fieldName Name of the field to examine.
    @return The field value as a boolean.
    @stability Evolving
 */
PUBLIC bool dbFieldBool(const DbItem *item, cchar *fieldName);

/**
    Fetch a field value from an item as a date.
    @description Use to examine an item returned via dbGet or other APIs that return items.
        This requires that the date value be stored as an ISO date string.
    @param item Database item returned from other APIs.
    @param fieldName Name of the field to examine.
    @return The field value as a date in a Time value. This is the time in milliseconds
        since Jan 1 1970.
    @stability Evolving
 */
PUBLIC Time dbFieldDate(const DbItem *item, cchar *fieldName);

/**
    Fetch a field value from an item as a double.
    @description Use to examine an item returned via dbGet or other APIs that return items.
        Converts numeric field values to double precision floating point format.
    @param item Database item returned from other APIs.
    @param fieldName Name of the field to examine.
    @return The field value as a double.
    @stability Evolving
 */
PUBLIC double dbFieldDouble(const DbItem *item, cchar *fieldName);

/**
    Find matching items in the database.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for matching items. Set to NULL if no model is required.
    @param props JSON object containing item properties to match. Use the macro DB_PROP(name, value,
       ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to provide the
       properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12 Wishbury
       lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are validated
       against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        int limit;        // Limit the number of returned or removed items.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
        cchar *next;      // Next pagination token to use as the starting point for the next page of
           results.
        \n
        DbWhere where;    // Where query expression callback function.
        \n
    @return A list of matching items.  Items can be enumerated or accessed using ITERATE_ITEMS,
       rGetNextItem and rGetItem.
        Caller must free the result using rFreeList.
    @stability Evolving
 */
PUBLIC RList *dbFind(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Find the first matching item.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for matching items. Set to NULL if no model is required.
    @param props JSON object containing item properties to match. Use the macro DB_PROP(name, value,
       ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to provide the
       properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12 Wishbury
       lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are validated
       against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
        DbWhere where;    // Where query expression callback function.
        \n
    @return The first matching item. Returns null if no match found.
    @stability Evolving
 */
PUBLIC const DbItem *dbFindOne(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Read a matching item from the database.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for matching items.
    @param props JSON object containing the item properties to match.
        Use the macro DB_PROP(name, value, ...) to specify the properties as a list of keyword /
           value pairs.
        Use DB_JSON to provide the properties as a JSON/5 string.
        For example: DB_PROP("name", name, "address", "12 Wishbury lane") or
        DB_JSON("{role: 'user'}"). If a model is provided, the properties are validated against the
        model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The matching database item. The DbItem "name" property is the indexed key.
        The value "json" property contains the item values as a cached JSON object.
        If null, the "value" property contains the item's value as an unparsed JSON string.
        Use dbField to access the individual field values in the item.
        Caller must not free the returned item.
    @stability Evolving
 */
PUBLIC const DbItem *dbGet(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Get a field from a matching item from the database.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for matching items.
    @param fieldName Name of the item field to return.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return A string containing the required field in the data item. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *dbGetField(Db *db, cchar *model, cchar *fieldName, Json *props, DbParams *params);

/**
   Get a field value from an item as a boolean
   @description If the field is not found, the default value is returned. Use dbGetField if you must know if the field
      is missing.
   @param db Database instance returned via #dbOpen
   @param model Name of the schema model for matching items.
   @param fieldName Name of the item field to return.
       @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
      value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
      provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
      Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
      validated against the model and unknown property names will not be written to the database.
   @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
   @param defaultValue Default value to return if the field is not found.
   @return The field value as a boolean.
   @stability Evolving
 */
PUBLIC bool dbGetBool(Db *db, cchar *model, cchar *fieldName, Json *props, DbParams *params, bool defaultValue);

/**
   Get a field value from an item as a date
   @description If the field is not found, the default value is returned. Use dbGetField if you must know if the field
      is missing.
   @param db Database instance returned via #dbOpen
   @param model Name of the schema model for matching items.
   @param fieldName Name of the item field to return.
       @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
      value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
      provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
      Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
      validated against the model and unknown property names will not be written to the database.
   @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
   @param defaultValue Default value to return if the field is not found.
   @return The field value as a date of seconds since epoch (Jan 1 1970).
   @stability Evolving
 */
PUBLIC Time dbGetDate(Db *db, cchar *model, cchar *fieldName, Json *props, DbParams *params, Time defaultValue);

/**
   Get a field value from an item as a double
   @description If the field is not found, the default value is returned. Use dbGetField if you must know if the field
      is missing.
   @param db Database instance returned via #dbOpen
   @param model Name of the schema model for matching items.
   @param fieldName Name of the item field to return.
       @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
      value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
      provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
      Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
      validated against the model and unknown property names will not be written to the database.
   @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
   @param defaultValue Default value to return if the field is not found.
   @return The field value as a double.
   @stability Evolving
 */
PUBLIC double dbGetDouble(Db *db, cchar *model, cchar *fieldName, Json *props, DbParams *params, double defaultValue);

/**
   Get a field value from an item as a number
   @description If the field is not found, the default value is returned. Use dbGetField if you must know if the field
      is missing.
   @param db Database instance returned via #dbOpen
   @param model Name of the schema model for matching items.
   @param fieldName Name of the item field to return.
       @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
      value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
      provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
      Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
      validated against the model and unknown property names will not be written to the database.
   @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
   @param defaultValue Default value to return if the field is not found.
   @return The field value as a number.
   @stability Evolving
 */
PUBLIC int64 dbGetNum(Db *db, cchar *model, cchar *fieldName, Json *props, DbParams *params, int64 defaultValue);

/**
    Get JSON object representing a database item
    @description Retrieve the parsed JSON representation of a database item.
    The returned JSON object provides access to all item fields and values.
    @param item Database item returned from dbCreate, dbGet, dbFind or dbUpdate
    @return JSON object containing item data. This is an internal reference - caller must NOT modify or free.
    Returns NULL if item is invalid.
    @stability Evolving
 */
PUBLIC const Json *dbJson(const DbItem *item);

/**
    Generate a Universally Unique Lexicographically Sortable Identifier (ULID)
    @description Creates a ULID based on the specified timestamp. ULIDs are 26-character
    case-insensitive strings that encode both timestamp and randomness, providing sortable unique identifiers.
    @param when Timestamp to embed in the ULID (milliseconds since epoch)
    @return Newly allocated ULID string. Caller must free with rFree.
    @stability Evolving
 */
PUBLIC char *dbGetULID(Time when);

/**
    Generate a unique identifier of specified length
    @description Creates a random unique identifier string of the requested size.
    Uses cryptographically secure random number generation.
    @param size Length of the generated UID string
    @return Newly allocated UID string. Caller must free with rFree.
    @stability Evolving
 */
PUBLIC char *dbGetUID(ssize size);

/**
    Convert item list to JSON string representation
    @description Serializes a list of database items into a JSON string format.
    Useful for exporting query results or transmitting data.
    @param list List of DbItem objects returned from dbFind
    @return Newly allocated JSON string containing the serialized items. Caller must free with rFree.
    Returns NULL if list is empty or invalid.
    @stability Evolving
 */
PUBLIC char *dbListToString(RList *list);

/**
    Convert database item to JSON string
    @description Serializes a database item into a JSON string with specified formatting options.
    @param item Database item returned from dbCreate, dbGet, dbFind or dbUpdate
    @param flags Formatting options: JSON_HUMAN (pretty-printed), JSON_JSON (strict JSON),
    JSON_JSON5 (JSON5 format), or other jsonToString flags
    @return JSON string representation. This is an internal reference - caller must not free.
    Returns NULL if item is invalid.
    @stability Evolving
 */
PUBLIC cchar *dbString(const DbItem *item, int flags);

/**
    Convert a list of keyword / value pairs into a JSON object
    @param props NULL terminated array of keyword / value pairs.
    @return A JSON object containing the supplied property values
    @stability Internal
 */
PUBLIC Json *dbPropsToJson(cchar *props[]);

/**
    Parse a string into Json properties.
    @param fmt Printf style format string
    @param ... Arguments to fmt
    @return A JSON object containing the supplied property values
    @stability Internal
 */
PUBLIC Json *dbStringToJson(cchar *fmt, ...);

/**
    Load data from JSON file into database
    @description Loads JSON data from a file into the database. Existing items with matching keys
    will be overwritten. This operation is useful for loading initial state data, test fixtures,
    or restoring database content from backups.
    @param db Database instance returned from dbOpen
    @param path Path to JSON data file to load
    @return Zero on success, negative error code on failure
    @stability Evolving
 */
PUBLIC int dbLoadData(Db *db, cchar *path);

/**
    Load data from JSON object into database
    @description Loads JSON data from a parsed JSON object into the database. Existing items
    with matching keys will be overwritten. This operation processes data items from the
    specified parent node in the JSON structure.
    @param db Database instance returned from dbOpen
    @param json Parsed JSON object containing data to load
    @param parent Parent JSON node containing the data items to load
    @return Zero on success, negative error code on failure
    @stability Evolving
 */
PUBLIC int dbLoadDataItems(Db *db, Json *json, JsonNode *parent);

/**
    Database flags
 */
#define DB_READ_ONLY  0x1   /**< Don't write to disk */
#define DB_OPEN_RESET 0x2   /**< Reset (erase) database on open */

/**
    Open a database
    @param path Filename for from which to load and save the database when calling dbSave. On open,
       an initial load is performed from the file at path.
    @param schema OneTable data schema describing the indexes and data models
    @param flags Reserved. Set to zero.
    @stability Evolving
    @see dbClose
 */
PUBLIC Db *dbOpen(cchar *path, cchar *schema, int flags);

/**
    Print database item to stdout
    @description Outputs a formatted representation of a database item to standard output.
    Useful for debugging and development.
    @param item Database item returned from dbCreate, dbGet, dbFind or dbUpdate
    @stability Evolving
 */
PUBLIC void dbPrintItem(const DbItem *item);

//  Internal
PUBLIC void dbPrintList(RList *list);

/**
    Print all database items to stdout
    @description Outputs a formatted representation of all items in the database to standard output.
    Useful for debugging and examining database contents during development.
    @param db Database instance returned from dbOpen
    @stability Evolving
 */
PUBLIC void dbPrint(Db *db);

/**
    Print JSON properties to stdout
    @description Outputs a formatted representation of JSON properties to standard output.
    Useful for debugging and examining property values during development.
    @param properties JSON object containing properties to display
    @stability Evolving
 */
PUBLIC void dbPrintProperties(Json *properties);

/**
    Remove matching items in the database.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for matching items.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        int limit;        // Limit the number of items to remove.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return A count of the number of items removed.
    @stability Evolving
 */
PUBLIC int dbRemove(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Save the database
    @param db Database instance
    @param filename Optional filename to save data to. If set to NULL, the data is saved to the name
       given when opening the database via #dbOpen.
    @stability Evolving
 */
PUBLIC int dbSave(Db *db, cchar *filename);

/**
    Update a database item field value as a boolean
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetBool(Db *db, cchar *model, cchar *fieldName, bool value, Json *props, DbParams *params);

/**
    Update a database item field value as a date.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetDate(Db *db, cchar *model, cchar *fieldName, Time value, Json *props, DbParams *params);

/**
    Update a database item field value as a double
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetDouble(Db *db, cchar *model, cchar *fieldName, double value, Json *props, DbParams *params);

/**
    Update a database item field value.
    @description Update a field in an existing item. The item must already exist. This sets the field value to the
       string value.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetField(Db *db, cchar *model, cchar *fieldName, cchar *value, Json *props, DbParams *params);

/**
    Update a database item field value as a number
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetNum(Db *db, cchar *model, cchar *fieldName, int64 value, Json *props, DbParams *params);

/**
    Update a database item field value as a string
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param fieldName Name of the item field to set.
    @param value Value to assign to the item's field.
    @param props JSON object containing the item properties to match. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free.
    @stability Evolving
 */
PUBLIC const DbItem *dbSetString(Db *db, cchar *model, cchar *fieldName, cchar *value, Json *props, DbParams *params);
/**
    Configure database journaling parameters
    @description Sets the journaling behavior for database persistence. The journal records
    changes before they are written to the main database file, providing crash recovery capability.
    Changes are automatically committed when either the delay time expires or the journal
    reaches the maximum size limit.
    @param db Database instance returned from dbOpen
    @param delay Maximum time in milliseconds to delay before committing journal changes to
    the persistent database file
    @param size Maximum journal file size in bytes before flushing to the persistent database file
    @stability Evolving
 */
PUBLIC void dbSetJournalParams(Db *db, Ticks delay, ssize size);

/**
    Add a database change trigger callback
    @description When database items are changed, the trigger callbacks are invoked to
        notify regarding the change.
    @param db Database instance
    @param proc Database trigger function
    @param model Target model. If null, then match all models.
    @param arg Argument to pass to the trigger function.
    @param events Events of interest. Set to DB_ON_CHANGE, DB_ON_COMMIT (or both).
    @stability Evolving
 */
PUBLIC void dbAddCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg, int events);

/**
    Remove a database change trigger callback
    @param db Database instance
    @param proc Database trigger function
    @param model Target model. If null, then match all models.
    @param arg Argument to pass to the trigger function.
    @stability Evolving
 */
PUBLIC void dbRemoveCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg);

//  DEPRECATED
#define dbSetCallback dbAddCallback

/**
    Get the type field of database items
    @param db Database instance returned via #dbOpen
    @return The type field used by database items. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *dbType(Db *db);

/**
    Update an item.
    @param db Database instance returned via #dbOpen
    @param model Name of the schema model for the item. Set to NULL if no model is required.
    @param props JSON object containing the item properties to update. Use the macro DB_PROP(name,
       value, ...) to specify the properties as a list of keyword / value pairs. Use DB_JSON to
       provide the properties as a JSON/5 string. For example: DB_PROP("name", name, "address", "12
       Wishbury lane") or DB_JSON("{role: 'user'}"). If a model is provided, the properties are
       validated against the model and unknown property names will not be written to the database.
    @param params List of API parameters. Use the macro DB_PARAMS(key = value, ...) to specify.
        \n
        cchar *index;     // Name of the index to use. Defaults to "primary". Currently only
           supports "primary".
        \n
    @return The updated item. Caller must not free. Returns null on errors. Use dbGetError to
       retrieve an error message.
    @stability Evolving
 */
PUBLIC const DbItem *dbUpdate(Db *db, cchar *model, Json *props, DbParams *params);

/**
    Get an error message for the most recent API call
    @param db Database instance returned via #dbOpen
    @return A static error message string. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *dbGetError(Db *db);

//  Internal
PUBLIC void dbPrintTree(Db *db);
PUBLIC void dbReset(cchar *path);

/**
    Get the model object for a model name
    @param db Database instance
    @param name Model name
    @return A model instance
    @stability Evolving
 */
PUBLIC DbModel *dbGetModel(Db *db, cchar *name);

/**
    Get the model object for a data item
    @param db Database instance
    @param item Data item with a model type field.
    @return A model instance
    @stability Evolving
 */
PUBLIC DbModel *dbGetItemModel(Db *db, CDbItem *item);

/**
    Get the next key when using paginated find requests.
    @param db Database instance
    @param list A list returned from a prior dbFind request
    @return A reference into the last item returned in the list
    @stability Evolving
 */
PUBLIC cchar *dbNext(Db *db, RList *list);

/**
    Remove expired database items
    @description Removes items from the database that have exceeded their TTL (time-to-live) expiration.
    Items are considered expired when their TTL field value is less than the current time.
    This operation helps maintain database efficiency by automatically cleaning up stale data.
    @param db Database instance returned from dbOpen
    @param notify Set to true to invoke registered callback notifiers for each removed item
    @return Number of items removed, or negative error code on failure
    @stability Prototype
 */
PUBLIC int dbRemoveExpired(Db *db, bool notify);

/**
    Compact database storage
    @description Optimizes database storage by converting parsed JSON items back to compact
    string representations. This reduces memory usage and improves storage efficiency by
    eliminating redundant parsed JSON structures while preserving all data.
    @param db Database instance returned from dbOpen
    @stability Evolving
 */
PUBLIC void dbCompact(Db *db);

/**
    Get the primary sort key field name
    @description Returns the name of the field used as the primary sort key for database items.
    This field determines the ordering of items in the primary index.
    @param db Database instance returned from dbOpen
    @return Name of the sort key field. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *dbGetSortKey(Db *db);
#ifdef __cplusplus
}
#endif

#endif /* ME_COM_DB */
#endif /* _h_DB */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
