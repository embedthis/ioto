/*
    eco.c - Sample Eco House demo app

    The Eco House app simulates a solar equipped house with battery and EV auto.
    There are five key models stored in the database: (See EcoSchema.json)

    Desired -- The desired car charging state as set by the App UI.
    State - The actual car charging state.
    Flow - The current flows to and from each component
    Charge - The stored charge in the house battery and EV battery.
    Capacity - The capacity in KWH of each component.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#define SOLAR_SIZE 14000            /* Solar panel size KW */
#define HOUSE_NEEDS 6000            /* House max consumption KW */

/************************************ Code ************************************/
/*
    Initialize the initial capacity table
 */
static void setupCapacity()
{
    /*
        Set the battery/car/solar capacities (KW)
        Upsert to update if already run before
    */
    if (dbCreate(ioto->db, "Capacity", DB_JSON("{battery: %d, car: %d, solar: %d}", 13500, 57500, 10000),
            DB_PARAMS(.upsert = 1)) == 0) {
        rError("eco", "Cannot create capacity table: %s", dbGetError(ioto->db));
    }
    /*
        Set initial car charging state. updateFlow() will notice if the Desired.car is charging.
     */
    if (dbGetField(ioto->db, "State", "car", NULL, NULL) == NULL) {
        if (dbCreate(ioto->db, "State", DB_JSON("{car: false}"), DB_PARAMS(.upsert = 1)) == 0) {
            rError("eco", "Cannot create state table: %s", dbGetError(ioto->db));
        }
    }
    rInfo("eco", "EcoHouse Database Initialized");
}

/*
    Update the current flow metrics 

    Negative vs Positive values indicate flow direction.
    E.g. -grid means sending current back to the grid.
 */
static void updateFlow(void *arg)
{
    Ticks   delay;
    bool    chargingCar;
    int     car, carCapacity, carCharge, battery, batteryCapacity, batteryCharge;
    int     grid, house, surplus, solar, store;

    delay = (Ticks) (ssize) arg;
    batteryCapacity = (int) stoi(dbGetField(ioto->db, "Capacity", "battery", NULL, NULL));
    batteryCharge = (int) stoi(dbGetField(ioto->db, "Charge", "battery", NULL, NULL));
    carCapacity = (int) stoi(dbGetField(ioto->db, "Capacity", "car", NULL, NULL));
    carCharge = (int) stoi(dbGetField(ioto->db, "Charge", "car", NULL, NULL));
    chargingCar = smatch(dbGetField(ioto->db, "Desired", "car", NULL, NULL), "true");

    //  Assume car has been driven
    carCharge -= max(0, min(carCharge, (rand() % carCapacity)) / 2);
    if (carCharge < 0) {
        carCharge = 0;
    }
    rInfo("eco", "Starting charge: battery %d, car %d, carState %s", 
        batteryCharge, carCharge, chargingCar ? "charging" : "not-charging");

    solar = rand() % SOLAR_SIZE;
    house = -(rand() % HOUSE_NEEDS);
    car = chargingCar ? -min(rand() % 4000, carCapacity - carCharge) : 0;
    grid = battery = 0;
    surplus = solar + car + house;

    rInfo("eco", "Flows: solar %d, car %d, house %d, surplus %d", solar, car, house, surplus);

    if (surplus > 0) {
        //  Excess solar generation - store what will fit in the battery
        store = min(batteryCapacity - batteryCharge, surplus);
        if (store > 0) {
            rInfo("eco", "Battery storing %d", store);
            batteryCharge += store;
            surplus -= store;
            battery = store;
        } 
        if (surplus > 0) {
            //  Send back to grid
            grid = -surplus;
        }
    } else {
        if (batteryCharge > 0) {
            if (-surplus <= batteryCharge) {
                //  Need is less than battary charge
                battery = surplus;
                batteryCharge += surplus;
                surplus = 0;
            } else {
                //  Need is greater than battary charge and will deplete the battery
                battery = -batteryCharge;
                grid = -surplus - batteryCharge;
                batteryCharge = 0;
                surplus += batteryCharge;
            }
            rInfo("eco", "Battery discharging %d", battery);
        }
        //  Get the rest from the grid
        grid = -surplus;
    }
    if (car < 0) {
        //  Add to the car's charge state
        carCharge += -car;
    }
    rInfo("eco", "Capacity: car %d, battery %d", carCharge, batteryCharge);

    if (grid < 0) {
        rInfo("eco", "Battery full, send excess %d back to the grid", grid);
    } else if (grid > 0) {
        rInfo("eco", "Battery depleted, draw %d from the grid", grid);
    } else {
        rInfo("eco", "Grid not required");
    }
    /*
        Update the flow table that will be sync'd to the cloud
        The Mobile app will read this item
     */
    if (dbUpdate(ioto->db, "Flow",
            DB_JSON("{battery: %d, car: %d, grid: %d, house: %d, solar: %d}", 
            battery, car, grid, house, solar),
            DB_PARAMS(.upsert = 1)) == 0) {
        rError("eco", "Cannot update capacity table value item in database: %s", dbGetError(ioto->db));
    }
    /*
        Update the state of charge for car and battery
        Not reflected in the demo App UI
     */
    if (dbUpdate(ioto->db, "Charge", DB_JSON("{car: %d, battery: %d}", carCharge, batteryCharge),
            DB_PARAMS(.upsert = 1)) == 0) {
        rError("eco", "Cannot update capacity table value item in database: %s", dbGetError(ioto->db));
    }
    rStartEvent(updateFlow, (void*) (ssize) delay, (int) delay);
}

/*
    Change the actual state to match the desired state
 */
static void ecoNotify(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *op)
{
    cchar   *actual, *desired;

    desired = dbField(item, "car");
    actual = dbGetField(ioto->db, "State", "car", NULL, NULL);

    if (smatch(desired, actual)) {
        rInfo("eco", "The car is already %s", desired ? "charging" : "not charging");
        return;
    }
    if (dbSetField(ioto->db, "State", "car", desired, NULL, NULL) == 0) {
        rError("eco", "Cannot set car state. Error %s.", dbGetError(ioto->db));
        return;
    }
    if (smatch(desired, "false")) {
        //  Immediately update the car flow to 
        if (dbSetNum(ioto->db, "Flow", "car", 0, NULL, NULL) == 0) {
            rError("eco", "Cannot set car flow to false. Error %s.", dbGetError(ioto->db));
            return;
        }
    }
    printf("\n%s charging car\n", smatch(desired, "true") ? "Start" : "Stop");
}

/*
    This is not used -- Just to demonstrate how MQTT commands can be receieved from automation triggers
 */
static void ecoCommand(MqttRecv *rp)
{
    rInfo("eco", "Got MQTT command %s, data %*s", rp->topic, rp->dataSize, rp->data);
}

static void ecoSetup()
{
    Ticks   delay;

    setupCapacity();

    /*
        Get notified when the app modifies the desired state of any element
     */
    dbAddCallback(ioto->db,  (DbCallbackProc) ecoNotify, "Desired", NULL, DB_ON_CHANGE);

    /*
        Start simulating the changing current flow 
    */
    delay = svalue(jsonGet(ioto->config, 0, "demo.delay", "30sec")) * TPS;
    rStartEvent(updateFlow, (void*) (ssize) delay, 0);

    //  This isn't used in this demo -- but shows how to listen to device commands
    //  rWatch("device:command:", (RWatchProc) deviceCommand, 0);

    //  This isn't used in this demo -- but illustrates how to subscribe for MQTT messages
    mqttSubscribe(ioto->mqtt, ecoCommand, 1, MQTT_WAIT_NONE, "ioto/device/%s/eco-house", ioto->id);
}

PUBLIC void ecoManager(void)
{
    static int  once = 0;

    if (once++ == 0) {
        ecoSetup();
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
