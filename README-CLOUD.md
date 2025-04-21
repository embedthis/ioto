# EmbedThis Ioto Cloud-based Device Managment

Please first read the [README.md](./README.md) for a general overview and for
build and configuration instructions.

If you are using building for
[ESP32](https://www.espressif.com/en/products/socs/esp32) hardware, please read
the [README-ESP32.md](README-ESP32.md) first.

Ioto offers extensive cloud-based management using the AWS IoT platform.
Integration is provided for cloud-based state persistency, provisioning, cloud
database sync and over-the-air device upgrading.

Full documentation is available at:

* https://www.embedthis.com/doc/

## Apps

The Ioto distribution build includes two demo apps for cloud-based management.
These Apps demonstrate the required device-side logic to communicate with the
cloud.

The **demo** app sends device data and metrics to the cloud.

## Configuration

When building Ioto, you do not need to use a `configure` program. Instead, you
simply run **make** and select your desired App. This will copy the App's
configuration and conditionally compile the required services based on the
App's **ioto.json5** configuration settings. During the first time build, the
App's configuration will be copied to the **state/config** directory.

You can provide parameters to the build via makefile command variables. See
below for details.

The selectable Ioto services are:

* database -- Enable the embedded database
* demo -- Demonstrate sending data to the cloud (set to mqtt or sync)
* keys -- Get AWS IAM keys for local AWS API invocation (dedicated clouds only)
* logs -- Capture log files and send to AWS CloudWatch logs (dedicated clouds
only)
* mqtt -- Enable MQTT protocol
* provision -- Dynamically provision keys and certificates for cloud based
management
* register -- Register the device with the Ioto Builder service
* serialize -- Run a serialization service when making the device
* shadow -- Enable AWS IoT shadow state storage
* sync -- Enable transparent database synchronization with the cloud
* test -- Enable the test framework
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

The **ioto.json5** configuration file has some conditional properties that are
applied depending on the selected **profile**. These properties are nested
under the **conditional** property and the relevant set are selected are copied
to overwrite properties of the same name at the top level. This allows a single
configuration file to apply different settings based on the current value of
the profile property.

If you are using ESP32, use the ESP-IDF menuconfig instead to modify the Ioto
component selected services.

## Registration

Devices register with the cloud on startup using a unique device ID and product
ID set in the **device.json5** config file. 

Ioto will generate a unique device ID for you automatically if not defined.
However, you can define your own ID in the **device.json5** file if you wish.
The device ID must be unique across all devices for your product type and so
should be at least 10 characters long.

Paste the value into the "id" property in the **device5.json file** and set
other device.json5 properties to your desired values.

The product ID is defined via the **"product"** property in the device.json5
file. 

During evalution, it is easiest to manage your divice use the pre-existing
**Eval cloud** and **Eval Device App**. The Eval cloud is a multi-tenant,
shared cloud for evaluating Ioto. The Eval Device App is a developer device app
suitable for examining and monitoring device data. You can use the eval product
token **"01H4R15D3478JD26YDYK408XE6"** which will be used when registering your
device with the eval cloud. Later, you can change this to be a product
definition of your own when you wish to use your own device cloud. For example:

```javascript
{
    id: 'YOUR-DEVICE-ID',
    product: '01H4R15D3478JD26YDYK408XE6',
    name: 'YOUR-PRODUCT-NAME',
    description: 'YOUR-PRODUCT-DESCRIPTION',
    model: 'YOUR-PRODUCT-MODEL'
}
```

## Managing Ioto Devices

When Ioto registers, it waits to be claimed (paired) by a user for management.
Until claimed, the device can only be locally managed via the embedded web
server.

An Ioto enabled device can be claimed by any user with the claim ID. When
claimed, the device is associated with a user, device app and underlying device
cloud. 

## Launching the Eval Device App

To launch the Eval App, navigate to the Builder and select the **App** page.

[![App
List](../../images/builder/app-list.png){.screen}](https://admin.embedthis.com/apps)

Then click the **Launch** icon for the **Eval** app. This will run the Eval
[Device App](https://www.embedthis.com/doc/apps/)

When you log into the device app, you are logging in as a device user. This is
a different account to your Builder login account. Device app logins are unique
for each device cloud.

![App Login](../../images/manager/developer-login.png){.screen .width-40}

Enter your desired username and password and click **Register**. You will be
sent an email confirmation code to complete your login.

## Claiming the Device

After logging in, you will see the device list, which will be initially empty.
Click the **Claim Device** button and enter your device claim ID to claim your
device for management.

![App Login](../../images/manager/device-claim.png){.screen}

The Claim ID is the unique device ID displayed by Ioto when run. This ID is
dynamically allocated when first run, but can be initialized via the
**device.json5** configuration file. During production, a generated device
claim ID is typically printed on the device packaging and device label. 

Read more in the [Builder Claiming
Devices](https://www.embedthis.com/doc/builder/devices/claiming/) documentation.

```
...
app: info: Device Claim ID: M72DANY8BZ
```

While an Ioto-enabled device is waiting to be claimed, the Ioto agent will
periodically check with the cloud service to see if it has been claimed. 

Once the device is claimed via the device app, Ioto will display provisioning
information. For example:

```bash
provision: info: Device claimed
provision: trace: {
    accountId: "XXXXXXXXXXXXXXXXXXXXXXXXXX",
    certificate: "state/ioto.crt",
    endpoint: "xxxxxxxxxxxxx-ats.iot.xxxxxxxxxxxxxx.amazonaws.com",
    id: "M72DANY8BZ",
    key: "state/ioto.key",
    port: 443
}
provision: info: Device provisioned
checkin: info: Device has no pending updates for version: 1.3.0
mqtt: info: Connected to mqtt:
xxxxxxxxxxxxx-ats.iot.xxxxxxxxxxxx-1.amazonaws.com:443
demo: info: Running demo cloud counter from start.c
demo: info: Send counter update via MQTT -- counter 0
```

When claimed, the Ioto agent will be provisioned with a unique device
certificate for secure TLS communications and will connect to the cloud using
the MQTT protocol. These will be saved under the **state** directory as
**ioto.crt** and **ioto.key**.

## Demo Cloud Messaging

The apps will typically send device data to the cloud for display in the app.
To view, 
select the claimed device from the device list.

![Device List](../../images/manager/device-list-1.png){.screen}

This will then display the database tables and dashboard for this device.

## Key/Value Store

The default device schema has a key/value table called the **"Store"**. The
demo counter, will update a value with the key of **"counter"**. Click on the
**Store** table to display the store contents.

This will display all the data items in the store for claimed devices. From
here, you can modify an item by clicking the **Edit** icon or you can edit in
place by clicking on a teal color cell and updating the value and then clicking
**Save**.

![Store Table](../../images/manager/store-table.png){.screen}

You can reload the table contents by clicking the table reload icon.

## App Dashboard

Device apps includes the ability to display one or more dashboards with data
widgets.

Click the **Dashboard** tab to display the default dashboard, then click the
**Add Widget** icon to display the add widget panel.

![Store Widget](../../images/manager/store-widget-add.png){.screen}

You can display database and metric data using numeric, gauge or graphical
widgets.

To display the demo counter as a numberic from the database, enter the
following widget configuration:

Field | Value
-|-
Type | Numeric
Namespace | Database
Model | Store
Field | value
Select Item | key=counter

This will select the Store table item that has a key value set to "counter" and
display the "value" field.

After the ioto agent has run for a minute, there will be sufficient data points
for a "COUNTER" metric to be created. Metrics permit the display of current and
historical data using graphical widgets.

To display the demo counter as a graph using metrics, enter the following
widget configuration:

Field | Value
-|-
Type | Graph
Namespace | Embedthis/Device
Metric | COUNTER
Statistic | avg
Resource Dimensions | Device=YOUR-CLAIM-ID

That should get you started with Ioto and posting data to the cloud from your
device.

Have a look at the **start.c** file for the code:

```c
static void demo(void)
{
    static int counter = 0;
    int        i;

    for (i = 0; i < 30; i++) {
        ioSetNum("counter", counter);
        rSleep(30 * TPS);
    }
}
```

### Create Your Own Device Cloud

As you gain experience with Ioto, you may wish to create your own device cloud
and apps your product with your logo, product name and device specific data,
screens, panels and inputs.

To do this, you must define a product definition via the Builder. To define
your own product, navigate to the [Builder
Products](https://admin.embedthis.com/products) menu option and click **Add**.
Select **Ioto** as your device agent.  From the product list, you can copy the
Product ID and replace the "product" property in the device.json5 file.

## Create a Device Cloud

To create a device cloud, navigate to the [Builder
Clouds](https://admin.embedthis.com/clouds) menu option and click **Add**. You
can choose to create a **hosted** device cloud which is hosted by EmbedThis in
an AWS account managed by EmbedThis.  Alternatively, you can create a dedicated
device cloud in an AWS account that you own.

## Creating a Device App

Once the device cloud is created, you can create a device app which can be
extensively customized with your product name, logo, UI components and logic.
The Eval device app is supplied with a **Developer** UI skin.

To download sample apps, download the Ioto Apps package from the [Builder
Products Page](https://admin.embedthis.com/products).

