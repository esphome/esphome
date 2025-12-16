---
description: "Instructions for setting up a web server in ESPHome."
title: "Web Server Component"
params:
  seo:
    description: Instructions for setting up a web server in ESPHome.
    image: http.svg
---

The `web_server` component creates a simple web server on the node that can be accessed
through any browser and a simple [REST API](/web-api#api-rest). Please note that enabling this component
will take up *a lot* of memory and may decrease stability, especially on ESP8266.

{{< img src="web_server.png" alt="Image" caption="Web server version 1" width="86.0%" class="align-center" >}}

{{< img src="web_server-v2.png" alt="Image" caption="Web server version 2" width="86.0%" class="align-center" >}}

{{< img src="web_server-v3.png" alt="Image" caption="Web server version 3" width="86.0%" class="align-center" >}}

To navigate to the web server in your browser, either use the IP address of the node or
use `<node_name>.local/` (note the trailing forward slash) via mDNS.

```yaml
# Example configuration entry
web_server:
  port: 80
```

## Configuration variables

- **port** (*Optional*, int): The port the web server should open its socket on.
- **css_url** (*Optional*, url): The URL that should be used for the CSS stylesheet. Defaults
  to <https://oi.esphome.io/v1/webserver-v1.min.css> (updates will go to `v2`, `v3`, etc). Can be set to empty string.

- **css_include** (*Optional*, local file): Path to local file to be included in web server index page.
  Contents of this file will be served as `/0.css` and used as CSS stylesheet by internal webserver.
  Useful when building device without internet access, where you want to use built-in AP and webserver.

- **js_url** (*Optional*, url): The URL that should be used for the JS script. Defaults
  to <https://oi.esphome.io/v1/webserver-v1.min.js>. Can be set to empty string.

- **js_include** (*Optional*, local file): Path to local file to be included in web server index page.
  Contents of this file will be served as `/0.js` and used as JS script by internal webserver.
  Useful when building device without internet access, where you want to use built-in AP and webserver.

- **auth** (*Optional*): Enables a simple *Digest* authentication with username and password.

  - **username** (**Required**, string): The username to use for authentication.
  - **password** (**Required**, string): The password to check for authentication.

- **include_internal** (*Optional*, boolean): Whether `internal` entities should be displayed on the
  web interface. Defaults to `false`.

- **enable_private_network_access** (*Optional*, boolean): Enables support for
  [Private Network Access](https://wicg.github.io/private-network-access) and the
  [Private Network Access Permission Prompt](https://wicg.github.io/private-network-access/#permission-prompt).
  Defaults to `true`.

- **log** (*Optional*, boolean): Turn on or off the log feature inside webserver. Defaults to `true`.
- **ota** (*Optional*, boolean): Explicitly disable OTA updates through the web server interface. Only accepts `false`.
  This option is typically used when you have both `web_server` and `captive_portal` configured, and you want
  OTA updates to be available only through the captive portal. Since `captive_portal` automatically loads the
  web server OTA platform, setting this to `false` prevents OTA access through the regular web interface while
  maintaining it for captive portal access. To enable OTA for web server, use the `web_server` OTA platform instead.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **local** (*Optional*, boolean): Include supporting javascript locally allowing it to work without internet access.
  Defaults to `false`.

- **version** (*Optional*, string): `1`, `2` or `3`. Version 1 displays as a table. Version 2 uses web components
  and has more functionality. Version 3 uses HA-Styling. Defaults to `2`.

- **sorting_groups** (*Optional*, list): Available only on `version: 3`. A list of group ID's and names to group the
  entities. See [Webserver Entity Grouping](#config-webserver-grouping).

  - **id** (**Required**, [ID](/guides/configuration-types#id)): Manually specify the ID used for the group.
  - **name** (**Required**, string): A string representing the group name which is displayed as the header of the group
  - **sorting_weight** (*Optional*, float): A float representing the weight of the group. A group with a smaller

  `sorting_weight` will be displayed first. Defaults to `50`

To conserve flash size, the CSS and JS files used on the root page to show a simple user
interface are externally hosted at oi.esphome.io. If you want to use your own service, use the
`css_url` and `js_url` options in your configuration.

> [!NOTE]
> **OTA Updates via Web Interface**
>
> The `ota` option has been moved from the `web_server` component to its own OTA platform.
>
> To enable OTA updates through the web interface, use the new `web_server` OTA platform:
>
> ```yaml
> # Enable OTA updates via web interface
> ota:
>   - platform: web_server
> ```
>
> To explicitly disable OTA updates for the web server while keeping them enabled for captive portal
> (useful when captive portal is configured since it automatically enables web server OTA):
>
> ```yaml
> # Disable OTA updates for web_server only
> # Captive portal will still have OTA access since it auto-loads the web server OTA platform
> web_server:
>   ota: false
>
> captive_portal:  # This component automatically enables OTA
> ```
>
> See {{< docref "/components/ota/web_server" >}} for more information.

## Example configurations

Enabling HTTP authentication:

```yaml
# Example configuration entry
web_server:
  port: 80
  auth:
    username: !secret web_server_username
    password: !secret web_server_password
```

> [!IMPORTANT]
> Always enable authentication when using the web server. See the [Security Best Practices](/guides/security_best_practices#2-web-server-authentication) guide for recommendations.

Use version 1 user interface:

```yaml
# Example configuration entry
web_server:
  port: 80
  version: 1
```

No internet/intranet required on the clients (all assets are inlined, compressed and served from flash):

```yaml
# Example configuration entry
web_server:
  local: true
```

Disabling OTA updates for web server while using captive portal (common security setup):

```yaml
# Example configuration entry
web_server:
  port: 80
  ota: false  # Disables OTA through regular web interface

# Captive portal automatically enables web server OTA platform
# OTA will only be accessible when captive portal is active
captive_portal:
```

## Advanced usage

The following assume copies of the files with local paths - which are config dependent.

Example `web_server` version 1 configuration with CSS and JS included from esphome-docs.
CSS and JS URL's are set to empty value, so no internet access is needed for this device to show it's web interface.

```yaml
web_server:
  port: 80
  version: 1
  css_include: "../../../esphome-docs/_static/webserver-v1.min.css"
  css_url: ""
  js_include: "../../../esphome-docs/_static/webserver-v1.min.js"
  js_url: ""
```

Example `web_server` version 2 configuration with JS included from a local file.
CSS and JS URL's are set to empty value, so no internet access is needed for this device to show it's web interface.
V2 embeds the css within the js file so is not required, however you could include your own CSS.

```yaml
# Example configuration entry v2
web_server:
  js_include: "./v2/www.js"
  js_url: ""
  version: 2
```

Copy <https://oi.esphome.io/v2/www.js> to a V2 folder in your yaml folder.

{{< anchor "config-webserver-version-3-options" >}}

## Version 3 features

{{< anchor "config-webserver-sorting" >}}

### Entity sorting

Version `3` supports the sorting of the entities.
You can set a `sorting_weight` on each entity.
Smaller numbers will be displayed first, defaults to 50.
`My Sensor 2` is displayed before `My Sensor 1`

Example configuration:

```yaml
sensor:
  - platform: template
    name: "My Sensor 1"
    web_server:
      sorting_weight: 10
  - platform: template
    name: "My Sensor 2"
    web_server:
      sorting_weight: -1
```

{{< anchor "config-webserver-grouping" >}}

### Entity grouping

Version `3` of the `web_server` allows for grouping of entities in custom groups.
Groups can be sorted by providing a `sorting_weight`. Groups with a smaller `sorting_weight` will be displayed first.
If you don't provide a `web_server_sorting_group` on the component, the `entity_category` will be used as the group.

Example configuration:

```yaml
web_server:
  version: 3
  sorting_groups:
    - id: sorting_group_time_settings
      name: "Time Settings"
      sorting_weight: 10
    - id: sorting_group_number_settings
      name: "Number settings"
      sorting_weight: 20

datetime:
  - platform: template
    ...
    web_server:
      sorting_group_id: sorting_group_time_settings

number:
  - platform: template
  ...
    web_server:
      sorting_group_id: sorting_group_number_settings
```

### Number in slider mode

{{< img src="number-slider-popup.png" alt="Image" width="100.0%" class="align-left" >}}

You can change the value by moving the slider.
If you wish to enter a precise number you can click and hold the current value. A popup input field will appear where
you can enter a number and confirm your input by pressing the enter key.

{{< img src="number-slider-popup-input-field.png" alt="Image" width="100.0%" class="align-left" >}}

### Expand Controls and Logs

{{< img src="tab-header-expand-cloapsed.png" alt="Image" width="100.0%" class="align-left" >}}

By double-clicking on any group header you can expand the controls to fill up the whole screen.
You can do the same for the logs.

{{< img src="tab-header-expand-controls-expanded.png" alt="Image" caption="Expanded Controls"
  width="100.0%" class="align-center" >}}

{{< img src="tab-header-expand-logs-expanded.png" alt="Image" caption="Expanded Logs"
  width="100.0%" class="align-center" >}}

### Sensor value graph

{{< img src="sensor-history-graph.png" alt="Image" width="100.0%" class="align-left" >}}

By clicking on any sensor it will expand a graph with the historical values for that sensor.

## See Also

- [Event Source API](/web-api#api-event-source)
- [REST API](/web-api#api-rest)
- {{< apiref "web_server/web_server.h" "web_server/web_server.h" >}}
- {{< docref "prometheus/" >}}
