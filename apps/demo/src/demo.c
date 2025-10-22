/*
    demo.c -- Demonstration App for Ioto
 */

/********************************* Includes ***********************************/
#include "ioto.h"

#if ESP32
#include "driver/gpio.h"
#include "rom/gpio.h"
#endif

/********************************* Defines ************************************/

#if ESP32
#define GPIO         2
#endif

/*
    SECURITY Acceptable:: This is the public eval product ID.
    Disclosure here is not a security risk.
 */
#define EVAL_PRODUCT "01H4R15D3478JD26YDYK408XE6"

/*
    Flag to indicate that on-demand MQTT connections are used
 */
static int onDemand = 0;

/*************************** Forward Declarations *****************************/

static void deviceCommand(void *ctx, DbItem *item);
static void customCommand(void *ctx, DbItem *item);
static void demo(void);

/*********************************** Code *************************************/
/*
    Called when Ioto is fully initialized. This runs on a fiber while the main fiber services events.
    Ioto will typically be connected to the cloud, but depending on the mqtt.schedule may not be.
    So we must use ioOnConnect to run when connected.
 */
int ioStart(void)
{
    rWatch("device:command:power", (RWatchProc) deviceCommand, 0);
    rWatch("device:command:custom", (RWatchProc) customCommand, 0);

    if (smatch(jsonGet(ioto->config, 0, "mqtt.schedule", 0), "unscheduled")) {
        onDemand = 1;
    }
    if (jsonGetBool(ioto->config, 0, "demo.enable", 0)) {
        if (onDemand) {
            demo();
        } else {
            //  Run the demo when the cloud MQTT connection is established
            ioOnConnect((RWatchProc) demo, 0, 1);
        }
        if (jsonGetBool(ioto->config, 0, "demo.service", 0)) {
            //  If offline, this update will be queued for sync to the cloud when connected
            dbUpdate(ioto->db, "Service", DB_JSON("{value: '%d'}", 0), DB_PARAMS(.upsert = 1));
        }
    } else {
        rInfo("demo", "Demo disabled");
    }
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
void ioStop(void)
{
    rWatchOff("device:command:power", (RWatchProc) deviceCommand, 0);
}

/*
    Main demonstration routine. Called when connected.
 */
static void demo(void)
{
    Ticks      delay;
    Time       expires;
    int        count, i;
    static int once = 0;
    static int counter = 0;

    if (once++ > 0) return;
    rInfo("demo", "Demo started\n");

    /*
        Get demo control parameters (delay, count)
     */
    delay = svalue(ioGetConfig("demo.delay", "30sec")) * TPS;
    count = ioGetConfigInt("demo.count", 30);
    rInfo("demo", "Running demo with %d iterations and delay of %d", count, (int) delay);

#if ESP32
    /*
        Toggle the LED to give visual feedback via GPIO pin 2
     */
    int level = 1;
    gpio_reset_pin(GPIO);
    gpio_set_direction(GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO, level);
#endif
    expires = rGetTime() + 2 * 3600 * TPS;
    dbRemoveExpired(ioto->db, 1);

    for (i = 0; i < count; i++) {
        if (!onDemand && !ioto->connected) {
            rInfo("demo", "Cloud connection lost, suspending demo");
            break;
        }
        rInfo("demo", "Demo iteration %d/%d", counter, count);
        rPrintf("\n");
        if (jsonGetBool(ioto->config, 0, "demo.counter", 0)) {
            rInfo("demo", "Updating Store.counter via MQTT request");
            ioSetNum("counter", counter);
        }

        if (jsonGetBool(ioto->config, 0, "demo.sync", 0)) {
            rInfo("demo", "Updating Store.counter via DB Sync");
            if (dbUpdate(ioto->db, "Store",
                         DB_JSON("{key: 'counter', value: '%d', type: 'number'}", counter),
                         DB_PARAMS(.upsert = 1)) == 0) {
                rError("demo", "Cannot update store item in database: %s", dbGetError(ioto->db));
            }
        }

        if (jsonGetBool(ioto->config, 0, "demo.metric", 0)) {
            //  Update a cloud metric called "RANDOM" via MQTT request
            double value = ((double) rand() / RAND_MAX) * 10;
            ioSetMetric("RANDOM", value, "", 0);

            //  Read the metric average for the last 5 minutes back from the cloud
            value = ioGetMetric("RANDOM", "", "avg", 5 * 60);
            rInfo("demo", "Random metric has average: %f", value);
        }

        if (!smatch(ioto->product, EVAL_PRODUCT)) {
            /*
                The Service and Log tables are defined in the custom schema.json5 file.
                Updates to these tables require a device cloud with the schema.json5 uploaded.
                Cannot be used on the eval cloud which is shared among all users.
             */
            if (jsonGetBool(ioto->config, 0, "demo.service", 0)) {
                rInfo("demo", "Updating Service table");
                if (dbUpdate(ioto->db, "Service", DB_JSON("{value: '%d'}", counter), DB_PARAMS(.upsert = 1)) == 0) {
                    rError("demo", "Cannot update service value item in database: %s", dbGetError(ioto->db));
                }
            }
            /*
                Update the cloud Log table with a new item.
                The expires field is optional and if not specified, the item will not be deleted.
             */
            if (jsonGetBool(ioto->config, 0, "demo.log", 0)) {
                rInfo("demo", "Updating Log table");
                if (dbCreate(ioto->db, "Log", DB_JSON("{message: 'message-%d', expires: '%ld'}", counter, expires),
                             DB_PARAMS()) == 0) {
                    rError("demo", "Cannot update log item in database: %s", dbGetError(ioto->db));
                }
            }
        }
        if (++counter < count) {
#if ESP32
            //  Trace task and memory usage
            rPlatformReport("DEMO Task Report");
#endif
            rSleep(delay);
        }
#if ESP32
        //  Toggle the LED to give visual feedback via GPIO pin 2
        level = !level;
        gpio_set_level(GPIO, level);
#endif
    }
    rInfo("demo", "Demo complete");
    rSignal("demo:complete");
}

/*
    Receive device commands from Device automations.
    These are sent via updates to the Command table.
 */
static void deviceCommand(void *ctx, DbItem *item)
{
    cchar *command;
    int   level;

    command = dbField(item, "command");
    level = (int) stoi(dbField(item, "args.level"));
    print("DEVICE command %s, level %d", command, level);
}

static void customCommand(void *ctx, DbItem *item)
{
    cchar *parameters, *program;
    int   status;

    program = dbField(item, "args.program");
    parameters = dbField(item, "args.parameters");

    /*
        WARNING: no error checking of program or parameters here
        SECURITY Acceptable:: This is demo code and is not used in production.
     */
#if ME_UNIX_LIKE
    char cmd[160], *output;
    print("Run custom command: %s %s", program, parameters ? parameters : "");
    SFMT(cmd, "%s %s", program, parameters ? parameters : "");
    status = rRun(cmd, &output);
    if (status != 0) {
        rError("demo", "Failed to run custom command: %s", output);
    }
    rFree(output);
#else
    rInfo("demo", "Not running custom command on non-Unix like platform");
#endif
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
