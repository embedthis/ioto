# Eco House App

The Eco House app is a simple demonstration cloud-based energy management app that emulates a house with solar panels, battery and an EV car. It provides configuration for the Ioto device agent, and a stylized mobile app for managing the "home". 

The Eco App is an example of a [Customized Device Manager](https://www.embedthis.com/manager/doc/config/levels/). This means it uses the standard Ioto Device Manager UI with a customized data schema, pages, dashboard and devic-cloud automations.

<img alt="Eco House Dashboard" src="https://www.embedthis.com/images/eco/eco-home.avif" width="400" /><br>

The Eco House App demonstrates:

* How to easily create an interactive mobile device manager UI.
* How to extend the Ioto device agent with custom data and logic.
* How to quickly create and deploy an IOT solution with the EmbedThis Builder.

This is not a "trivial" sample. To develop this sample from scratch without the Builder and Ioto would typically take many months of development with associated risk.

This sample will:

* Download and build a device agent with custom logic managing an ECO House.
* Create a regional device cloud to manage devices.
* Create and host a device manager app.
* Create a custom Eco House UI and control panel dashboard.
* Connect user actions from the app with custom logic to control the Eco House.

## Device Agent

The Eco House app extends the Ioto agent by providing an extension code module, database schema and agent configuration. In a "real" home, the Ioto agent with Eco extension would be run on an embedded device such as a [Raspberry PI](https://www.raspberrypi.com/) with wired connections to the solar panels, battery, gateway and EV chargers. For this sample, we run the device agent on a PC/Notebook to simulate a real Eco house embedded device.

## Device Manager

The Eco House app uses the unmodified, Standard Device Manager UI available from the Builder &mdash; so you don't need to build or upload a manager UI app. 

The Eco House app is designed to run on a mobile device, but can also be used on a desktop.

## Steps

<!-- no toc -->
- [Create Product](#create-product)
- [Download Agent](#download-agent)
- [Build Agent](#build-agent)
- [Create Device Cloud](#create-device-cloud)
- [Create Device Manager](#create-device-manager)
- [Run Agent](#run-agent)
- [Launch Device Manager](#launch-device-manager)
- [Claim Device](#claim-device)
- [Import Dashboard](#import-dashboard)
- [Create Automation](#create-automation)

### Create Product

The first step is to create an Eco House product definition in the [Builder](https://admin.embedthis.com/).

Navigate to the [Builder](https://admin.embedthis.com/clouds) site and select `Products` in the sidebar menu and click `Add Product`. Then create a product definition by entering a product name and description of your choosing. Select the `Ioto Agent` and select `By Device Volume` and enter `1` in the Total Device field. Your first device is free.

<img src="https://www.embedthis.com/images/eco/eco-product-edit.avif" alt="Create Eco Product" width="400"><br>

### Download Agent

Once the product definition is created, you can click `Download` from the Products list and save the source distribution to your system. The eval version of Ioto will be fine for this solution.

<img src="https://www.embedthis.com/images/builder/product-list.avif" alt="Product List"><br>

Take note of the `Product ID` in the product listing. You can also click on the product ID to copy it to the clipboard. You will enter this product ID in the Ioto configuration file: `apps/eco/config/device.json5`. 

### Build Agent

To build the Ioto agent with Eco extensions, first extract the source files from the downloaded archive:

    $ tar xvfz ioto-eval-src.tgz

Before building, edit the `apps/eco/config/device.json5` file and paste in the Product ID into the `product` property obtained from the Builder.
 
Then build Ioto with the Eco app, by typing:

    $ make APP=eco

This will build Ioto, the Eco app and will copy the Eco config files to the top-level `./config` directory.

### Create Device Cloud 

Before running Ioto, you need to create a Device Cloud for your agent to communicate with. The device cloud manages communication with devices and stores device data. 

To create a device cloud, navigate to the [Builder Clouds List](https://admin.embedthis.com/clouds) by selecting `Clouds` from the side menu. Then click `Add Cloud`. Enter your desired cloud name, and select `Hosted by Embedthis` in a region close to you. You can create the cloud and connect one device for free.

Check the `Upload Schema` option and upload the `state/schema.json5` file from your extracted Ioto source code. This schema defines the database model entities for the Eco House app.

### Create Device Manager

To view your device state, you need to create a Device Manager that will host the Eco App UI. This will create your Eco House App UI and host it globally on the EmbedThis Ioto device cloud.

Select `Managers` from the Builder side menu and click `Add Manager`. Enter your desired manager name (EcoHouse) and pick a domain name for your Eco app. The domain will be a subdomain of the `ioto.me` domain and will be automatically registered and published for you. Later, if you create a dedicated device cloud, you can select your own custom domain with any TLD extension.

You can specify an app logo for your device manager. For now, you can use the `apps/eco/config/eco-logo.avif` logo file.

The Eco App uses the Standard device manager UI navigation with a custom dashboard UI. In the future, if you wish to completely customize the UI, you can modify, rebuild or replace the portions or the whole of the underlying manager app with your own custom app.

The Standard device manager is a VueJS app that provides the following components:

* Login and auth
* Navigation
* Device claiming
* Dashboards & widgets
* Device Metrics and analytics
* Device data display and tables
* Alerts
* Responsive mobile & desktop presentation
* Dark/light mode support

After creating the manager, you need to wait a few minutes (and sometimes up to 30 minutes) to let the domain name entries propagate globally. While waiting, you can start the Ioto agent.

### Run Agent

The easiest way to run the Ioto agent with Eco House extensions is to type:

```bash
$ make run
```

In the console output, you will see a unique device ID displayed. This is a `Device Claim ID` that you can use to claim the device for exclusive management by your Eco House app. Take note of that device claim ID.

When Ioto starts, it will register with the Builder and wait to be claimed by your Eco House App.

### Launch Device Manager

From the Builder manager list, click the "Manage" column to launch your device manager. This will launch your default browser and navigate to the domain URL you chose when creating the manager.

<img src="https://www.embedthis.com/images/builder/manager-list.avif" alt="Manager List"><br>

Once launched, you will need to register and create a new "end-user" account with the Device Manager. 

> Note: this is not the same as your Builder login. 

<img src="https://www.embedthis.com/images/eco/eco-login.avif" alt="ECO Login" width="400"><br>

Enter a username and password and click register. A registration code will be emailed to you. Enter that code in the next screen to complete the registration.

### Claim Device

Once logged in, you can `claim` your device.

Select `"Devices"` from the sidebar menu and click `Claim Device` and then enter the claim ID shown in the Ioto agent console output. 

<img src="https://www.embedthis.com/images/eco/eco-claim.avif" alt="ECO Claim" width="600"><br>

The Ioto agent will poll regularly to see if it has been claimed. After starting, the Ioto delay between polling gradually increases. If the agent has been running a long time, the polling period may be up to 1 minute in length. You can restart the agent to immediately check with the Builder.

### Import Dashboard

After claiming, you can import the Eco house dashboard from `./config/Display.json5`. 

Select `Dashboards` from the sidebar menu and click `Actions/Import` and select the `./config/Dashboard.json5` file. 

After loading the dashboard, you can select the EcoHouse dashboard from the Dashboard list to display the Eco Dashboard.

Note: you can remove the Default dashboard at any time as it is not used by this sample.

### Create Automation

To respond to user actions in the Eco House App, you create a `Builder Automation Action` and `Trigger`. Then when the user clicks an Eco House App button, a request is sent to the device cloud which invokes the trigger and action. 

In the Eco House sample, the `Charge Car` button is connected to a database update action in the device cloud. The Eco extensions in the Ioto agent subscribe to watch changes to this table and can react when the user wants to charge the car.

To configure a Builder action, navigate to the Builder `Automations` page and select the `Actions` tab, select your device cloud and click `Add Action`. Give your action a name like "Database" and select `Database` from the action type list. Some actions take addition parameters, but the database action does not.

Then select the `Triggers` tab and click `Add Trigger`. 

<img alt="Eco Trigger" src="https://www.embedthis.com/images/eco/eco-trigger-add.avif" width="400" /><br>

Select `"User Trigger"` as the Trigger Source and select the name of your Action created above as the Action. Select `Upsert` and `"Desired"` as the database entity.

Now, when a user clicks the `Charge Car` button in the Eco App, that will send a message to the Automation trigger which will update the `Desired` database entity. The Eco App defines which field to update in the button widget. The imported Dashboard will have this already configured. 

If you are interested, you can put the dasboard into development mode by disabling `Fixed Design` and `Frameless Widgets` in the Dashboard edit panel.

<img alt="Eco Dashboard" src="https://www.embedthis.com/images/eco/eco-dash-edit.avif" width="400" /><br>

Then you can modify a widget configuration by clicking the "Pencil" icon at the top right of the widget. This will display the Widget properties panel.

<img alt="Eco Widget" src="https://www.embedthis.com/images/eco/eco-widget-action.avif" width="400" /><br>

## How It Works

The following section provides a background on some of the design of the Eco House app.

### Device Agent

The Ioto device agent is extended via an Eco House module. There are three files:

File | Description
-|-
eco.h | Eco House header
eco.c | Implementation of the ioStart and ioStop routines 
ecoApp.c | Code for the Eco House extension

This module uses the Ioto `ioStart` and `ioStop` hooks to start and stop the extension. When linked with the Ioto agent library, these hooks replace the stub functions and are called by Ioto on startup and shutdown.

The ioStart routine checks if the `demo.enable` property is true in the `ioto.json5` configuration file. If true, it schedules the `ecoManager` routine to run when Ioto connects to the cloud by using the `ioOnConnect` API.

### Database 

The database schema is used by both the Ioto agent and device cloud to define the Eco House data app entities (models). 

The `apps/eco/schema.json5` defines the overall schema and the `EcoSchema.json5` file defines the Eco House specific portions.

The underlying agent database and the cloud database are based on the [AWS DynamoDB](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/Introduction.html) database which is a highly scalable, high-performance, NoSQL, fully managed database.

There are 5 database entity models (think tables). These are:

Name | Sync Direction | Purpose
-|-|-
Capacity | up | Stores battery capacity of the home and EV batteries
Charge | up | Stores the battery charge of the home and EV batteries
Desired | down | Stores the desired state of charging the EV
Flow | up | Stores the current flows to and from all components
State | up | Stores the current charging state of the EV

The Ioto database synchronization automatically replicates data up to the cloud and down to the device according to the sync direction. You do not need to explicitly send data to or from the cloud (unless you want to). Ioto database replication does this transparently, reliably and efficiently.

When the `Charge Auto` button in the Eco House App is clicked, the associated widget action sets the `Desired.car` field to true. This change is then replicated down to the device agent and the Eco House extension is notified of the change. 

### Device Manager 

The Eco House App is an example of a [Styled Device Manager](https://www.embedthis.com/manager/doc/config/levels/). This means it adds device data schema, custom dashboards to the standard Ioto Device Manager, and utilizes cloud-side automated actions to react to user and device input.

The Eco house device manager provides a highly usable, responsive mobile and desktop UI.

### Modifying a Dashboard

The Eco House dashboard can be modified by selecting `Edit` from the dasboard list in the Eco app. Then disable `Fixed` and disable `Frameless Widgets`. This will display the dashboard toolbar and make it easy to see the widget size so that you can move and resize widgets. Using the toolbar, you can add or remove widgets and style individual widgets.

## Directories

| Directory | Purpose                      |
| --------- | -----------------------------|
| config    | Configuration files          |
| src       | Eco House App C source code  |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/Dashboard.json5    | Primary Ioto configuration file           |
| config/EcoSchema.json5    | Eco House database schema file            |
| config/ioto.json5         | Primary Ioto configuration file           |
| schema.json5       | Complete database schema file             |
| src/*.c                   | Device-side app service code              |
