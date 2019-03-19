# Mongoose OS MQTT Queue

[![Gitter](https://badges.gitter.im/cesanta/mongoose-os.svg)](https://gitter.im/cesanta/mongoose-os?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

- [Mongoose OS MQTT Queue](#mongoose-os-mqtt-queue)
  - [Author](#author)
  - [Features](#features)
  - [Settings](#settings)
  - [Installation/Usage](#installationusage)
    - [Use specific branch of library](#use-specific-branch-of-library)
  - [How it works](#how-it-works)
  - [Available Functions/Methods](#available-functionsmethods)
    - [C Functions](#c-functions)
  - [Changelog](#changelog)
  - [License](#license)

This is a MQTT queue library for Mongoose OS to queue MQTT publish events when the device is disconnected, writing the JSON to a file, and then once the device reconnects it will resend the the MQTT event.

This library was built specifically for Mongoose OS GCP (Google Cloud Platform/Google Core IoT), but could be easily adapted for standard MQTT servers or other providers pretty easily.

**This is still in beta stages and needs to be thoroughly tested**

## Author
Myles McNamara ( https://smyl.es )

## Features
- Set maximum files to create, after reaching that limit, the oldest will be overwritten
- On reconnect, will republish MQTT event
- After publishing queued event the file will be deleted from the device
- Set custom directory for storing queue data files (must be created as another partition)

## Settings
Check the `mos.yml` file for latest settings, all settings listed below are defaults

```yaml
  - [ "gcp.queue.enable", "b", true, {title: "Enable MQTT Queue on device boot (to process queue and set event handler on boot) "}]
  - [ "gcp.queue.max", "i", 10, {title: "Maximum number of files to keep in queue (if queue exceeds this limit, oldest files will be overwritten)"}]
  - [ "gcp.queue.delay", "i", 5, {title: "Delay between sending each queueud MQTT event upon reconnect"}]
  - [ "gcp.queue.data", "s", "", {title: "Custom location to store queue files (only should be used if you have created ext partition that is mounted)"}]
```

## Installation/Usage
Add this lib your `mos.yml` file under `libs:`

```yaml
  - origin: https://github.com/tripflex/mqtt-queue-gcp
```

### Use specific branch of library
To use a specific branch of this library (as example, `dev`), you need to specify the `version` below the library

```yaml
  - origin: https://github.com/tripflex/mqtt-queue-gcp
   version: dev
```

## How it works
The ideal flow for this library is that you will call this function to send the MQTT event, regardless of whether the device is connected or not.  Instead of calling `mgos_gcp_send_event_subf` you would call `mgos_mqtt_queue_gcp_send_event_subf`.  If the device is connected to GCP, it will use the native handling exactly the same as `mgos_gcp_send_event_subf`.  Ideally you would only call this for important MQTT events that must be sent once reconnected, using the standard `mgos_gcp_send_event_subf` for events that are not of top priority to send.

One file is created to store the index data, `queuemeta.json`

If the device is **not connected** this library will instead create two files (`%d` will be replaced with an integer -- the index of the queue):
- `queue_%d.json` - Fully parsed and generated JSON to send
- `queue_%d_meta.json` - JSON containing the `subfolder` and any other meta necessary for sending the event

As example, if you have set `gcp.queue.max` to `10`, that means a total of `20` files will be created, `10` for data and `10` for meta, and will start at `1`.  So the first time a queue is created the files will be `queue_1.json` and `queue_1_meta.json`, and then the next one will be `queue_2.json` and `queue_2_meta.json`, and so on.

If the index reaches the max set in settings, it will then go back to `1` (the oldest) and will overwrite `queue_1.json`, etc etc until the device reconnects.

When the device reconnects, it will read the `queuemeta.json` file to get the current index, it will then work backwards from the latest index file (ie `queue_5.json` if that was the last created), sending the event data, and removing the file, updating the new index to (`index-1`)

## Available Functions/Methods

### C Functions
```C
bool mgos_mqtt_queue_gcp_send_event_subf(const char *subfolder, const char *json_fmt, ...);
```

## Changelog

**1.0.0** TBD - Initial release

## License
Apache 2.0
