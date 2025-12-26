---
description: "Getting Started guide for installing ESPHome using the command line and creating a basic configuration."
title: "Getting Started with the ESPHome Command Line"
params:
  seo:
    description: Getting Started guide for installing ESPHome using the command line and creating a basic configuration.
    image: console.svg
---

ESPHome is the perfect solution for creating custom firmwares for
your ESP8266/ESP32 boards. In this guide we'll go through how to set up a
basic “node” in a few simple steps.

## Installation

See {{< docref "installing_esphome/" >}}.

If you're familiar with Docker, you can use that instead!
Note that on macOS Docker [can not pass USB devices through](https://github.com/moby/hyperkit/issues/149).
You will not be able to flash ESP devices through USB on Mac, all other features will work. Flashing with web
dashboard is still possible.

Our image supports AMD64, ARM and ARM64 (AARCH64), and can be downloaded with:

```bash
docker pull ghcr.io/esphome/esphome
```

If you want to use `docker-compose` instead, here's a sample file:

```yaml
version: '3'
services:
  esphome:
    container_name: esphome
    image: ghcr.io/esphome/esphome
    volumes:
      - /path/to/esphome/config:/config
      - /etc/localtime:/etc/localtime:ro
    restart: always
    privileged: true
    network_mode: host
    environment:
      - USERNAME=test
      - PASSWORD=ChangeMe
```

> [!NOTE]
> If you are using NFS share to back your container's config volume, you may
> need to mount the volume with the `nolock` option, otherwise platformio may
> freeze on container startup as per [platformIO-core Issue 3089](https://github.com/platformio/platformio-core/issues/3089)

> [!WARNING]
> Running ESPHome in Docker on WSL2 can be significantly slower
> (10x or more) than native Linux or a traditional VM due to filesystem performance
> issues when accessing files on Windows drives. For better performance, store your
> ESPHome files inside the WSL2 filesystem (e.g., `~/esphome/...`)
> rather than on a Windows mount (e.g., `/mnt/c/...`). See [Issue #12568](https://github.com/esphome/esphome/issues/12568)
> for more details.

The project provides multiple docker tags; please pick the one that suits you
better:

- `latest` and `stable` point to the latest stable release available. It's
  not recommended to automatically update the container based on those tags
  because of the possible breaking changes between releases.

- Release-tracking tag `YEAR.MONTH` (e.g. `2022.8`  ) points to the latest
  stable patch release available within the required version. There should
  never be a breaking change when upgrading the containers based on tags like
  that.

- `beta` points to the latest released beta version, and to the latest stable
  release when there is no fresh beta release.

- `dev` is the bleeding edge release; built daily based on the latest changes
  in the `dev` branch.

## Connecting the ESP Device

Follow the instructions in {{< docref "physical_device_connection/" >}} to connect to your
ESP device.

> [!NOTE]
> The most difficult part of setting up a new ESPHome device is the initial
> installation. Installation requires that your ESP device is connected with
> a cable to a computer. Later updates can be installed wirelessly.

## Creating a Project

Now let's setup a configuration file. Fortunately, ESPHome has a
friendly setup wizard that will guide you through creating your first
configuration file. For example, if you want to create a configuration
file called `livingroom.yaml`  :

```bash
esphome wizard livingroom.yaml
# On Docker:
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome wizard livingroom.yaml
```

At the end of this step, you will have your first YAML configuration
file ready. It doesn't do much yet and only makes your device connect to
the WiFi network, but still it's a first step.

## Adding some features

So now you should have a file called `livingroom.yaml` (or similar).
Go open that file in an editor of your choice and let's add a
{{< docref "/components/switch/gpio" "simpleGPIO switch" >}} to our app.

```yaml
switch:
  - platform: gpio
    name: "Living Room Dehumidifier"
    pin: GPIO5
```

The configuration format should hopefully immediately seem similar to
you. ESPHome has tried to keep it as close to Home Assistant's
`configuration.yaml` schema as possible. In the above example, we're
simply adding a switch that's called “Living Room Dehumidifier” (could control
anything really, for example lights) and is connected to pin `GPIO5`.
The nice thing about ESPHome is that it will automatically also try
to translate pin numbers for you based on the board. For example in the
above configuration, if using a NodeMCU board, you could have just as
well set `D1` as the `pin:` option.

## First uploading

Now you can go ahead and add some more components. Once you feel like
you have something you want to upload to your ESP board, simply plug in
the device via USB and type the following command (replacing
`livingroom.yaml` with your configuration file):

```bash
esphome run livingroom.yaml
```

You should see ESPHome validating the configuration and telling you
about potential problems. Then ESPHome will proceed to compile and
upload the custom firmware. You will also see that ESPHome created a
new folder with the name of your node. This is a new PlatformIO project
that you can modify afterwards and play around with.

If you are running docker on Linux you can add `--device=/dev/ttyUSB0`
to your docker command to map a local USB device. Docker on Mac will not be able to access host USB devices.

```bash
docker run --rm --privileged -v "${PWD}":/config --device=/dev/ttyUSB0 -it ghcr.io/esphome/esphome run livingroom.yaml
```

> [!NOTE]
> Alternatively, you can flash the binary using [ESPHome Web or esptool](/guides/faq#esphome-esptool).

Now when you go to the Home Assistant **Integrations** screen (under **Configuration** panel), you
should see the ESPHome device show up in the discovered section (although this can take up to 5 minutes).
Alternatively, you can manually add the device by clicking **CONFIGURE** on the ESPHome integration
and entering `<NODE_NAME>.local` as the host.

{{< img src="gpio-ui.png" alt="Image" class="align-center" >}}

After the first upload, you will probably never need to use the USB
cable again, as all features of ESPHome are enabled remotely as well.
No more opening hidden boxes stowed in places hard to reach. Yay!

## Adding A Binary Sensor

Next, we're going to add a very simple binary sensor that periodically
checks if a particular GPIO pin is pulled high or low - the
{{< docref "/components/binary_sensor/gpio" "GPIO BinarySensor" >}}.

```yaml
binary_sensor:
  - platform: gpio
    name: "Living Room Window"
    pin:
      number: 16
      inverted: true
      mode:
        input: true
        pullup: true
```

This is an advanced feature of ESPHome. Almost all pins can
optionally have a more complicated configuration schema with options for
inversion and pinMode - the [Pin Schema](/guides/configuration-types#pin-schema).

This time when uploading, you don't need to have the device plugged in
through USB again. The upload will magically happen “over the air”.
Using ESPHome directly, this is the same as from a USB cable, but
for docker you need to supply an additional parameter:

```bash
esphome run livingroom.yaml
# On docker
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome run livingroom.yaml
```

{{< img src="gpio-ui.png" alt="Image" >}}

## Where To Go Next

Great 🎉! You've now successfully set up your first ESPHome project
and uploaded your first ESPHome custom firmware to your node. You've
also learned how to enable some basic components via the configuration
file.

So now is a great time to go take a look at the {{< docref "/index" "Components Index" >}}.
Hopefully you'll find all sensors/outputs/etc. you'll need in there. If you're having any problems or
want new features, please either create a new issue on the
[GitHub issuetracker](https://github.com/esphome/esphome/issues) or find us on the
[Discord chat](https://discord.gg/KhAMKrd) (also make sure to read the {{< docref "faq" "FAQ" >}}).

{{< anchor "esphome-device-builder-docker" >}}

## Bonus: ESPHome Device Builder

The ESPHome Device Builder allows you to easily manage your nodes from a nice web interface. It was primarily designed
as a {{< docref "getting_started_hassio" "Home Assistant add-on" >}}, but can run in docker independently from
Home Assistant.

To start the ESPHome Device Builder, simply start ESPHome with the following command (with `config/` pointing to a
directory where you want to store your configurations):

```bash
# Install dashboard dependencies
pip install tornado esptool

# Start the dashboard
esphome dashboard config

# On Docker, host networking mode is required for online status indicators
docker run --rm --net=host -v "${PWD}":/config -it ghcr.io/esphome/esphome

# On Docker with MacOS, the host networking option doesn't work as expected. An
# alternative is to use the following command if you are a MacOS user.
docker run --rm -p 6052:6052 -e ESPHOME_DASHBOARD_USE_PING=true -v "${PWD}":/config -it ghcr.io/esphome/esphome
```

After that, you will be able to access the ESPHome Device Builder at `localhost:6052`.

Logging level can be set with the env var `ESPHOME_LOG_LEVEL` (default is `INFO`).

{{< img src="dashboard_states.png" alt="Image" >}}

## See Also

- {{< docref "cli/" >}}
- {{< docref "/index" "ESPHome index" >}}
- {{< docref "getting_started_hassio/" >}}
- {{< docref "security_best_practices" >}}
