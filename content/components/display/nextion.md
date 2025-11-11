---
description: "Instructions for setting up Nextion TFT LCD displays"
title: "Nextion TFT LCD Display"
params:
  seo:
    description: Instructions for setting up Nextion TFT LCD displays
    image: nextion.jpg
---

The `nextion` display platform allows you to use Nextion LCD displays
([datasheet](https://nextion.itead.cc/resources/datasheets/), [iTead](https://www.itead.cc/display/nextion.html))
with ESPHome.

{{< img src="nextion-full.jpg" alt="Image" caption="Nextion display" width="75.0%" class="align-center" >}}

Communication with the Nextion display is done via a serial interface, so you'll need to have a [UART Bus](/components/uart)
in your configuration with both `rx_pin` and `tx_pin` configured. These pins must then be connected to the
respective pins on the display.

Nextion displays use a baud rate of 9600 by default. You may configure the Nextion display to use a higher speed by
editing the `program.s` source file in the Nextion Editor. For example:

```c
baud=115200   // Sets the baud rate to 115200; for other supported rates, see https://nextion.tech/instruction-set/
bkcmd=0       // Tells the Nextion to not send responses on commands. This is the current default but can be set just in case
```

This permits faster communication with the Nextion display and it is highly recommended when using
[Hardware UARTs](/components/uart#uart-hardware_uarts).

> [!WARNING]
> **We highly recommend using only** [Hardware UARTs](/components/uart#uart-hardware_uarts) **with Nextion displays.**
>
> *Use of software UARTs is known to result in unpredictable/inconsistent behavior.*
>
> If you **must** use a software UART, note that baud rates greater than 9600 are extremely likely to cause problems.
>
> In short, avoid using software UARTs with Nextion displays.

```yaml
# Example configuration entry
display:
  - platform: nextion
    id: nextion1
    lambda: |-
      it.set_component_value("gauge", 50);
      it.set_component_text("textview", "Hello World!");
```

## Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [UART Bus](/components/uart) you wish to use for this display. Specify this
  when you have multiple UART configurations.

- **brightness** (*Optional*, percentage): When specified, the display brightness will be set to this value at boot.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the Nextion
  display. See [Rendering Lambda](#display-nextion_lambda) for more information. This is typically empty. The individual components
  for the Nextion will handle almost all functions needed for updating display elements.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to call the lambda to update the display.
  Defaults to `5s`.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **tft_url** (*Optional*, string): The URL from which to download the TFT file for display firmware updates (Nextion
  OTA). See [Nextion Upload](#nextion_upload_tft).

- **touch_sleep_timeout** (*Optional*, int): Sets internal No-touch-then-sleep timer in seconds.
  Range: 0 (disabled) or 3-65535 seconds (max: ~18 hours). Values 1-2 are auto-corrected to 3.
  When set, Nextion will automatically enter sleep mode after the specified period of no touch activity.
  This setting persists until device reboot or reset. Note: The display will only wake up by restart or by configuring `auto_wake_on_touch: true`.

- **start_up_page** (*Optional*, int): Sets the page to display when ESPHome connects to the Nextion. (Nextion shows page 0 on start-up by default).
- **wake_up_page** (*Optional*, int): Sets the page to display after waking up
- **exit_reparse_on_start** (*Optional*, boolean): Request the Nextion exit Active Reparse Mode before setup of the display. Defaults to `false`.
- **on_setup** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed after ESPHome connects to the Nextion. See [Nextion Automation](#nextion-on_setup).
- **on_sleep** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed when the Nextion goes to sleep. See [Nextion Automation](#nextion-on_sleep).
- **on_wake** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed when the Nextion wakes up. See [Nextion Automation](#nextion-on_sleep).
- **on_page** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed after a page change. See [Nextion Automation](#nextion-on_page).
- **on_touch** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed after a touch event (press or release). See [Nextion Automation](#nextion-on_touch).
- **auto_wake_on_touch** (*Optional*, boolean): If set to `true`, the Nextion will be configured to wake from sleep
  when touched.

- **skip_connection_handshake** (*Optional*, boolean): Sets whether the initial display connection handshake process is
  skipped. When set to `true`, the connection will be established without performing the handshake. This can be
  useful when using Nextion Simulator. Defaults to `false`.

- **on_buffer_overflow** (*Optional*, [Action](/automations/actions#all-actions)): An action to be performed when the Nextion
  reports a buffer overflow. See [Nextion Automation](#nextion-on_buffer_overflow).

- **command_spacing** (*Optional*, [Time](/guides/configuration-types#time)): Sets the minimum time between commands sent to the Nextion display.
  A higher value can help prevent buffer overflows but will result in slower interface updates.
  Range is `0-255ms`. Defaults to `0ms` (disabled).

- **max_commands_per_loop** (*Optional*, integer): Limits the number of commands processed per loop cycle.
  This helps prevent stack overflows when a large number of commands are queued.
  Lower values (for example, `20`  ) may help improve stability in constrained environments.

- **max_queue_size** (*Optional*, integer): Sets the maximum number of commands that can be queued at once.
  When the limit is reached, new commands will be dropped and a warning will be logged.
  This helps prevent memory overflows or boot-time crashes in complex setups that issue a large number of commands
  in rapid succession. If not set, the queue size is unlimited.

- **dump_device_info** (*Optional*, boolean): Shows device information (model, firmware version, serial number, flash size) in the configuration dump.
  When disabled, device info is only logged during connection establishment to save memory. Defaults to `false`.

{{< anchor "display-nextion_lambda" >}}

## Rendering Lambda

Nextion displays have a dedicated processor built directly into the display to perform all rendering. ESPHome simply
sends *instructions* to the display to tell it *how* to render something and/or *what* to render.

First, you need to use the [Nextion Editor](https://nextion.tech/nextion-editor/) to create a "TFT" display file and
"install" it onto the display, typically via an SD card onto which you'll copy the "TFT" file and then insert into the
display for installation/updating. Then, in the rendering `lambda`, you can use the various API calls to populate the
display with data:

```yaml
display:
  - platform: nextion
    # ...
    lambda: |-
      // set the "value" of a component - value is always an integer
      // for example gauges represent their status using integers from 0 to 100
      it.set_component_value("gauge", 50);

      // set the text of a component
      it.set_component_text("textview", "Hello World!");

      // set the text of a component with formatting
      it.set_component_text_printf("textview", "The uptime is: %.1f", id(uptime_sensor).state);
```

> [!NOTE]
> Although you can use the rendering lambda, most, if not all, updates to the Nextion can be handled by the
> individual Nextion components/platforms. **See Below**

See [Formatted Text](/components/display#display-printf) for a quick introduction to the `printf` formatting rules and [Displaying Time](/components/display#display-strftime) for
an introduction to `strftime` time formatting.

### Using Lambdas

Several methods are available for use within [lambdas](/automations/templates#config-lambda); these permit advanced functionality beyond
simple display updates. There are too many to cover here; please see the {{< apiref "nextion/nextion.h" "nextion/nextion.h" >}} for more detail.
The list below calls out a few commonly-used methods:

{{< anchor "nextion_upload_tft" >}}

- `upload_tft`  : Start the process to upload a new TFT file to the Nextion; see [Uploading A TFT File](#nextion_upload_tft_file) below.

{{< anchor "nextion_update_all_components" >}}

- `update_all_components()`  : All the components will publish their states.

```c
    id(nextion1).update_all_components();
```

{{< anchor "update_components_by_prefix" >}}

- `update_components_by_prefix(std::string page)`  : This will send the current state of any **component_name**
  matching the prefix. Some settings like background color need to be resent on page change; this is a good hook to use
  for that.

```c
    id(nextion1).update_components_by_prefix("page0.");
```

{{< anchor "set_nextion_sensor_state" >}}

- Set various sensor states (See [Queue Types](#queue-types) below):

  - `set_nextion_sensor_state(NextionQueueType queue_type, std::string name, float state);`
  - `set_nextion_sensor_state(int queue_type, std::string name, float state);`
  - `set_nextion_text_state(std::string name, std::string state);`

> [!NOTE]
> The example below demonstrates how to define a user-API so Home Assistant can send updates to the Nextion by code.
>
> ```yaml
> # Enable Home Assistant API
> api:
>   actions:
>     - action: set_nextion_sensor
>       variables:
>         nextion_type: int
>         name: string
>         state: float
>       then:
>         - lambda: |-
>             id(nextion1).set_nextion_sensor_state(nextion_type,name,state);
>     - action: set_nextion_text
>       variables:
>         name: string
>         state: string
>       then:
>         - lambda: |-
>             id(nextion1).set_nextion_text_state(name,state);
> ```

### Queue Types

| Type              | Value |
| ----------------- | ----- |
| `SENSOR`          | `0`   |
| `BINARY_SENSOR`   | `1`   |
| `SWITCH`          | `2`   |
| `TEXT_SENSOR`     | `3`   |
| `WAVEFORM_SENSOR` | `4`   |
| `NO_RESULT`       | `5`   |

{{< anchor "display-nextion_automation" >}}

## Nextion Automations

### Triggers

Several [Triggers](/automations/actions#actions-trigger) are available for use with your Nextion display.

{{< anchor "nextion-on_setup" >}}

#### `on_setup`

This automation will be triggered when a connection is established with the Nextion display. This happens after boot
and it may take some time (hundreds of milliseconds). It could be used to change some display element once start-up is
complete. For example:

```yaml
wifi:
  ap:  # Spawn an AP with the device name and MAC address with no password

captive_portal:

display:
  - platform: nextion
    id: disp
    on_setup:
      then:
        - lambda: |-
            // Check if WiFi hot-spot is configured
            if (wifi::global_wifi_component->has_sta()) {
              // Show the main page
              id(disp).goto_page("main_page");
            } else {
              // Show WiFi Access Point QR code for captive portal, see https://qifi.org/
              id(disp).goto_page("wifi_qr_page");
            }
```

{{< anchor "nextion-on_sleep" >}}

#### `on_sleep` / `on_wake`

These automations will be triggered upon sleep or upon wake (respectively). The Nextion does not accept commands or
updates while in sleep mode; these triggers may be used to cope with this. For example, you could use them to
[force an update](#nextion_update_all_components), refreshing the display's content upon wake-up.

{{< anchor "nextion-on_page" >}}

#### `on_page`

This automation is triggered when the page is changed on display. This includes both ESPHome-initiated and
Nextion-initiated page changes. ESPHome initiates a page change by calling either the `goto_page("page_name")` or
`goto_page(page_id)` functions. The Nextion itself can also change pages as a reaction to user activity (touching
some display UI element) or by using a timer. In either case, this automation can be useful to update on-screen
controls for the newly displayed page.

If you fully own your Nextion HMI design and follow the best practice of setting `vscope` to "global" for UI
components you've defined in the Nextion Editor, you'll probably never need this trigger. However, if this is not the
case and some/all of your UI components have their `vscope` set to "local", `on_page` will be your remedy -- it
enables you to initiate updates of the relevant components.

Before updating components, you need to know which page the Nextion is displaying. The `x` argument will contain an
integer which indicates the current page ID number.

Given the page ID, the appropriate components can be updated. Two strategies are be possible:

- Use [Nextion Sensors](/components/sensor/nextion#nextion_sensor) for every UI field and use one of the
  [update functions](#nextion_update_all_components).

- Manually set component text or value for each field:

```yaml
    on_page:
      then:
        - lambda: |-
            switch (x) {
              case 0x02: // wifi_qr_page
                // Manually trigger update for controls on page 0x02 here
                id(disp).set_component_text_printf("qr_wifi", "WIFI:T:nopass;S:%s;P:;;", wifi::global_wifi_component->get_ap().get_ssid().c_str());
                break;
            }
```

{{< anchor "nextion-on_touch" >}}

#### `on_touch`

This automation is triggered when a component is pressed or released on the Nextion display.

The following arguments will be available:

- `page_id`  : Contains the ID (integer) of the page where the touch happened.
- `component_id`  : Contains the ID (integer) of the component touched. **You must have "Send Component ID" enabled

    for "Touch Press Event" and/or "Touch Release Event" for the UI element in your HMI configuration in the**
    [Nextion Editor](https://nextion.tech/nextion-editor/).

- `touch_event`  : It will be `true` for a "press" event, or `false` for a "release" event.

```yaml
on_touch:
  then:
    - lambda: |-
        ESP_LOGD("nextion.on_touch", "Nextion touch event detected!");
        ESP_LOGD("nextion.on_touch", "Page ID: %i", page_id);
        ESP_LOGD("nextion.on_touch", "Component ID: %i", component_id);
        ESP_LOGD("nextion.on_touch", "Event type: %s", touch_event ? "Press" : "Release");
```

{{< anchor "nextion-on_buffer_overflow" >}}

#### `on_buffer_overflow`

This automation is triggered when the Nextion display reports a serial buffer overflow. When this happens, the
Nextion's buffer will continue to receive the new instructions, but all previous instructions are lost and the Nextion
queue may get out of sync.

This automation will allow you to gracefully handle this situation; for example, you could repeat some command/update
to the Nextion or restart the system.

```yaml
on_buffer_overflow:
  then:
    - lambda: |-
        ESP_LOGW("nextion.on_buffer_overflow", "Nextion reported a buffer overflow event!");
```

### Actions

{{< anchor "nextion-set_brightness" >}}

#### `display.nextion.set_brightness`

You can use this [action](/automations/actions#actions-action) to set the brightness of the Nextion's backlight.

```yaml
on_...:
  then:
    - display.nextion.set_brightness: 50%
```

Or, if you happen to have multiple Nextion displays connected, you may need to use the long form:

```yaml
on_...:
  then:
    - display.nextion.set_brightness:
        id: nextion1
        brightness: 50%
```

{{< anchor "nextion_upload_tft_file" >}}

## Uploading A TFT File

This will use the file specified for `tft_url` to update ("OTA") the Nextion.

Once completed, both ESPHome and the Nextion will reboot. ESPHome will be unresponsive during the upload process and no
logging or other {{< docref "/automations/index" "automations" >}} will occur. This process uses the same protocol as the
[Nextion Editor](https://nextion.tech/nextion-editor/) and only transfers required portions of the TFT file.

> [!WARNING]
> *Use of software UARTs is known to result in unpredictable/inconsistent behavior and will likely result in the
> update process failing.*
>
> If you experience problems with the update process and are using a software UART (for example, on the ESP8266), you
> should switch to an ESP32 or supported variant which has more available [Hardware UARTs](/components/uart#uart-hardware_uarts).

You can use Home Assistant itself or any other web server to host the TFT file. When using HTTPS (generally
recommended), you may notice reduced upload speeds as the encryption consumes more resources on the microcontroller.

We suggest using a {{< docref "/components/button/template" >}} to trigger this process. For example:

```yaml
button:
  - platform: template
    id: update_nextion_button
    name: Update Nextion
    entity_category: diagnostic
    on_press:
      then:
        - lambda: 'id(nextion1)->upload_tft();'
```

### Home Assistant

To host the TFT file from Home Assistant, create a `www` directory (if it doesn't already exist) in your `config`
directory. If you wish, you may also create a subdirectory for your TFT files.

For example, if the file is located in your configuration directory `www/tft/default.tft`, the URL to access it will
be `http(s)://your_home_assistant_url:port/local/tft/default.tft`

## Components

This library supports a few different components allowing communication between Home Assistant, ESPHome and Nextion.

> [!NOTE]
> If the Nextion is sleeping or if the component was set to be hidden, it will not update its components even if
> updates are sent. To work around this, after the Nextion wakes up, all components will send their states to the
> Nextion.

With the exception of the {{< docref "../binary_sensor/nextion" >}} that has the `page_id`  /`component_id` options configured,
the example below illustrates:

- Polling the Nextion for updates
- Dynamic updates sent from the Nextion to ESPHome

```yaml
  sensor:
    - platform: nextion
      nextion_id: nextion1
      name: "n0"
      component_name: n0
    - platform: nextion
      id: current_page
      name: "current_page"
      variable_name: dp
      update_interval: 1s
```

Note that the first one requires a custom protocol to be included in the Nextion display's HMI code/configuration. See
the individual components (linked below) for more detail.

## See Also

- {{< docref "index/" >}}
- {{< docref "../binary_sensor/nextion" >}}
- {{< docref "../sensor/nextion" >}}
- {{< docref "../switch/nextion" >}}
- {{< docref "../text_sensor/nextion" >}}
- {{< docref "../uart" >}}
- {{< apiref "nextion/nextion.h" "nextion/nextion.h" >}}
- [Simple Nextion Library](https://github.com/bborncr/nextion) by [Bentley Born](https://github.com/bborncr)
- [Official Nextion Library](https://github.com/itead/ITEADLIB_Arduino_Nextion) by [iTead](https://www.itead.cc/)
