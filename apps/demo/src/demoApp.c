/*
    demoApp.c -- Demonstration App for Ioto
 */
#include "ioto.h"

#if ESP32
#include "driver/gpio.h"
#include "rom/gpio.h"
#define GPIO 2
#endif

static void deviceCommand(void *ctx, DbItem *item);
static void demo(void);

/*
    Called when Ioto is fully initialized. This runs on a fiber while the main fiber services events.
    Ioto will typically be connected to the cloud, but depending on the mqtt.schedule may not be.
    So we must use ioOnConnect to run when connected.
 */
int ioStart(void)
{
    rWatch("device:command:power", (RWatchProc) deviceCommand, 0);

    //  Read settings from the ioto.json5 config file
    if (jsonGetBool(ioto->config, 0, "demo.enable", 0)) {
        ioOnConnect((RWatchProc) demo, 0, 1);
        /*
            If offline, this update will be queued for sync to the cloud when connected
         */
        dbUpdate(ioto->db, "Service", DB_JSON("{value: '%d'}", 0), DB_PARAMS(.upsert = 1));
        rInfo("demo", "Demo started\n");
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
    Ticks       delay;
    cchar       *demo;
    int         count, i;
    static int  once = 0;
    static int  counter = 0;
    
    if (once++ > 0) return;
    
    /*
        Get demo control parameters (delay, count)
     */
    demo = ioGetConfig("demo.type", "mqtt");
    delay = svalue(ioGetConfig("demo.delay", "30sec")) * TPS;
    count = ioto->cmdCount ? ioto->cmdCount : ioGetConfigInt("demo.count", 30);
    rInfo("demo", "Running demo \"%s\" count %d, delay %d", demo, count, (int) delay);

#if ESP32
    /*
        Toggle the LED to give visual feedback via GPIO pin 2
     */
    int level = 1;
    rInfo("demo", "Running demo \"%s\"", demo);
    gpio_reset_pin(GPIO);
    gpio_set_direction(GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO, level);
#endif

    for (i = 0; i < count; i++) {
        /*
            Could reconnect if disconnected via ioConnect()
         */
        if (!ioto->connected) {
            rError("demo", "Cloud connection lost, suspending demo");
            break;
        }

        /*
            Perform the selected demo as selected via the ioto.json5 demo collection. 
            These demonstrate different aspects of the Ioto service.
         */
        rInfo("demo", "Send counter %d/%d using demo \"%s\" with delay %d", counter, count, demo, (int) delay);
        if (smatch(demo, "service")) {
            /*
                Update the cloud Service table with the counter via database sync
             */
            if (dbUpdate(ioto->db, "Service", DB_JSON("{value: '%d'}", counter), DB_PARAMS(.upsert = 1)) == 0) {
                rError("demo", "Cannot update service value item in database: %s", dbGetError(ioto->db));
            }
        } else if (smatch(demo, "sync")) {
            /*
                Update the cloud Store table's counter item via database sync
             */
            if (dbUpdate(ioto->db, "Store",
                DB_JSON("{key: 'counter', value: '%d', type: 'number'}", counter),
                DB_PARAMS(.upsert = 1)) == 0) {
                rError("demo", "Cannot update store item in database: %s", dbGetError(ioto->db));
            }
        } else if (smatch(demo, "mqtt")) {
            /*
                Update the cloud Store table's counter item via MQTT request
             */
            ioSetNum("counter", counter);

        } else if (smatch(demo, "log")) {
            /*
                Update the cloud Log table with a new item.
             */
            if (dbCreate(ioto->db, "Log", DB_JSON("{message: 'when-%d'}", rGetTime()), NULL) == 0) {
                rError("demo", "Cannot update log item in database: %s", dbGetError(ioto->db));
            }
        } else if (smatch(demo, "metric")) {
            //  Update the "RANDOM" cloud metric for display in the manager
            double value = ((double) random() / RAND_MAX) * 10;
            ioSetMetric("RANDOM", value, "", 0);
            value = ioGetMetric("RANDOM", "", "avg", 5 * 60);
        }
        if (++counter < count) {
#if ESP32
            //  Trace task and memory usage
            rPlatformReport("DEMO Task Report");
#endif
            rSleep(delay);
        }
#if ESP32
        level = !level;
        gpio_set_level(GPIO, level);
#endif
    }
    rInfo("demo", "Demo complete");
    rSignal("demo:complete", 0);

    //  The app will now exit
    rStop();
}

/*
    Receive device commands from Device automations. 
    These are sent via updates to the Command table. 
 */
static void deviceCommand(void *ctx, DbItem *item)
{
    cchar    *command;
    int      level;

    command = dbField(item, "command");
    level = (int) stoi(dbField(item, "args.level"));
    print("DEVICE command %s, level %d", command, level);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

