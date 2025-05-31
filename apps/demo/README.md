# Demo App

The Demo app is a simple cloud-based app that demonstrates sending data to the device cloud.

The Demo App is an example of a [Customized Device App](https://www.embedthis.com/doc/apps/patterns/). This means it adds a device data schema to the standard device app.

The Demo app uses the standard device app UI with a customized data schema and custom device metrics.

<img alt="Demo Dashboard" src="https://www.embedthis.com/images/demo/demo-home.avif" 
class="screen" /><br>

The Demo App demonstrates

* How to download and build the Ioto agent with custom extensions
* How to create a custom database schema
* How to create a device cloud and device app
* How to send device data to the cloud

This sample will:

* Download and build the Ioto device agent with custom logic.
* Create a regional device cloud.
* Create a device app.
* Create a custom metric based on the device data.

## Device Agent

The Demo app extends the Ioto agent by providing an extension code module, database schema and agent configuration. 

## Device App

The Demo app uses the unmodified, standard Ioto device app &mdash; so you don't need to build or upload a UI app. 

The Demo app UI is designed to run on a mobile device, but can also be used on a desktop.

## Steps

<!-- no toc -->
- [Create Product](#create-product)
- [Download Agent](#download-agent)
- [Build Agent](#build-agent)
- [Create Device Cloud](#create-device-cloud)
- [Create Device App](#create-device-app)
- [Run Agent](#run-agent)
- [Launch Device App](#launch-device-app)
- [Claim Device](#claim-device)
- [Device Data](#device-data)
- [Data Metrics](#data-metrics)
- [Data Widgets](#data-widgets)

### Create Product

The first step is to create an Demo product definition in the [Builder](https://admin.embedthis.com/).

Navigate to the [Builder](https://admin.embedthis.com/clouds) site and select `Products` in the sidebar menu and click `Add Product`. Then create a product definition by entering a product name and description of your choosing. Select the `Ioto Agent` and select `Evaluation` as the plan type. 

<img src="https://www.embedthis.com/images/demo/demo-product-edit.avif" alt="Create Demo Product" width="600"><br>

### Download Ioto Agent

Once the product definition is created, you can click `Download` from the Products list by selecting the download icon for your "Demo App" and save the source distribution to your system. 

<img src="https://www.embedthis.com/images/builder/product-list.avif" alt="Product List"><br>

Take note of the `Product ID` in the product listing. You can also click on the product ID to copy it to the clipboard. You will enter this product ID in the Ioto configuration file: `apps/demo/config/device.json5`. 

### Build Agent

To build the Ioto agent with Demo extensions, first extract the source files from the downloaded archive:

    $ tar xvfz ioto-eval-src.tgz

Before building, edit the `apps/demo/config/device.json5` file and paste in the Product ID into the `product` property.
 
Then build Ioto with the Demo app, by typing:

    $ make APP=demo

This will build Ioto, the Demo app and will copy the Demo config files to the top-level `state/config` directory. Make sure when you want to make config changes, you make them to the 'apps/*/config' directories and not to the top-level `state/config` directory as they will be overwritten when you build Ioto.


### Create Device Cloud 

Before running Ioto, you need to create a Device Cloud for your agent to communicate with. The device cloud manages communication with devices and stores device data. 

To create a device cloud, navigate to the [Builder Clouds List](https://admin.embedthis.com/clouds) by selecting `Clouds` from the side menu. Then click `Add Cloud`. Enter your desired cloud name, and select `Hosted by Embedthis` in a region close to you. You can create the cloud and connect one device for free.

Check the `Upload Schema` option and upload the `./schema.json5` file from your extracted Ioto source code. This schema defines the database model entities for the Demo app.

### Create Device App

To view your device state, you need to create a device app that will display the Demo UI. This will create a standard device app and host it on the EmbedThis Ioto device cloud.

Select `Apps` from the Builder side menu and click `Add Apps`. Enter your desired app name (Demo) and pick a domain name for your app. The domain will be a subdomain of the `ioto.me` domain and will be automatically registered and published for you. Later, if you create a dedicated device cloud, you can select your own custom domain with any TLD extension.

The Demo App uses the standard DevCore UI. In the future, if you wish to customize the UI, you can modify, rebuild or replace the portions or the whole of the underlying app with your own custom app.

The standard DevCore UI is a VueJS app that provides the following components:

* Login and auth
* Navigation
* Device claiming
* Dashboards & widgets
* Device Metrics and analytics
* Device data display and tables
* Alerts
* Responsive mobile & desktop presentation
* Dark/light mode support

After creating the app, you need to wait a few minutes (and sometimes up to 30 minutes) to let the domain name entries propagate globally. While waiting, you can start the Ioto agent.

### Run Agent

The easiest way to run the Ioto agent with Demo extensions is to type:

```bash
$ make run
```

When Ioto is run, the Demo app will run a demo routine that updates the `Service` table with a counter every 30 seconds. The demo is configurable via the `ioto.json5` "demo" configuration collection. The demo can be configured to tailor the delay, number of updates and the mechanism used to perform updates. See comments in the `demoApp.c` for details.

In the console output, you will see a unique device ID displayed. This is generated if not already defined in the `device.json5` file. This ID is a `Device Claim ID` that you can use to claim the device for exclusive management by your Demo app. Take note of that device claim ID.

When Ioto starts, it will register with the Builder and wait to be claimed by your Demo App.

### Launch Device App

From the Builder apps list, click the "Launch" icon to launch your device app. This will launch your default browser and navigate to the domain URL you chose when creating the app.

<img src="https://www.embedthis.com/images/builder/manager-list.avif" alt="Apps List"><br>

Once launched, you will need to register and create a new account with the app. This is an "end-user" account for the owner of the device.

> Note: this is not the same as your Builder login. 

<img src="https://www.embedthis.com/images/demo/demo-login.avif" alt="Demo Login" width="400"><br>

Enter a username and password and click register. A registration code will be emailed to you. Enter that code in the next screen to complete the registration.

### Claim Device

Once logged in, you can `claim` your device.

Select `Devices` from the sidebar menu and click `Claim Device` and then enter the claim ID shown in the Ioto agent console output. 

<img src="https://www.embedthis.com/images/demo/demo-claim.avif" alt="Demo Claim" width="600"><br>

The Ioto agent will poll regularly to see if it has been claimed. After starting, the Ioto delay between polling gradually increases. If the agent has been running a long time, the polling period may be up to 1 minute in length. You can restart the agent to immediately check with the Builder.

### Device Data

After claiming the device for management, you will start to see device data in the device app. Navigate to `Device Data` and you will see a list of database models. If you are from a SQL background, database models are somewhat akin to SQL tables.

<img src="https://www.embedthis.com/images/demo/demo-table-list.avif" alt="Table List" width="600"><br>

From the table list, click on the `Service` table to display its contents. 

<img src="https://www.embedthis.com/images/demo/demo-service-table.avif" alt="Service Table" width="600"><br>

You can edit values inline, your select a row and click edit to display the item edit panel.

<img src="https://www.embedthis.com/images/demo/demo-table-edit.avif" alt="Table Edit" width="600"><br>

### Data Metrics

You can automatically create data metrics from device data so that you can track and manage device data over time.  Metrics can be used to provide data for dashboard widgets such as graphs and gauges. Metrics can also be used to invoke automatic actions.

<img alt="Demo Trigger" src="https://www.embedthis.com/images/demo/demo-trigger-add.avif" width="600" /><br>

To create a device metric, navigate to the Builder `Automations` page and select the `Actions` tab, give your action a name and description and select `Database Update` as your trigger source. Select the table `Service`. This table is updated by the demo app embedded in the Ioto agent.

For the action parameters, select `App Metric` as the Action and enter "COUNTER" as the Metric Name and "Device=${deviceId}" as the Metric Dimensions and "${value}" as the Metric Value.

This will create a metric named "COUNTER" for each device and use the `value` field from the Service table as the metric value.


### Data Widgets

You can display device data as graphical widgets from the app dashboard. Click `Home` to display the dashboard.

Then click the `+` plus icon to open the `Create Widget` panel and select "Graph" as the Widget type from the selection list. Then click the `Data` tab at the top to configure the data source for the widget.

Select `Database` as the namespace, `Service` as the table name and `value` as the item field. 

Then click `Save`. This will display your graph on the dashboard.

<img alt="Demo Dashboard" src="https://www.embedthis.com/images/demo/demo-home.avif" /><br>

## How It Works

The following section provides a background on some of the design and implementation of the Demo app.

### Device Agent

The Ioto device agent is extended via an Demo module. There are three files:

File | Description
-|-
demoApp.c | Code for the Demo extension

This module uses the Ioto `ioStart` and `ioStop` hooks to start and stop the extension. When linked with the Ioto agent library, these hooks replace the stub functions and are called by Ioto on startup and shutdown.

The ioStart routine checks if the `demo.enable` property is true in the `ioto.json5` configuration file. If true, it schedules the `evalDemo` routine to run when Ioto connects to the cloud by using the `ioOnConnect` API.

### Database 

The database schema is used by both the Ioto agent and device cloud to define the Demo data app entities (models). 

The `apps/demo/schema.json5` defines the overall schema and the `DemoSchema.json5` file defines the Demo specific portions.

The underlying agent database and the cloud database are based on the [AWS DynamoDB](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/Introduction.html) database which is a highly scalable, high-performance, NoSQL, fully managed database.

There are 2 database tables. These are:

Name | Sync Direction | Purpose
-|-|-
Service | up | A service-level table with a single item.
Log | up | Log table that creates new items with each update.

The Ioto database synchronization automatically replicates data up to the cloud and down to the device according to the sync direction. You do not need to explicitly send data to or from the cloud (unless you want to). Ioto database replication does this transparently, reliably and efficiently.

## Directories

| Directory | Purpose                      |
| --------- | -----------------------------|
| config    | Configuration files          |
| src       | Demo App C source code       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/DemoSchema.json5   | Demo database schema file                 |
| config/ioto.json5         | Primary Ioto configuration file           |
| schema.json5              | Complete database schema file             |
| src/*.c                   | Device-side app service code              |
