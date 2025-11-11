---
description: "Instructions for setting up base covers in ESPHome."
title: "Cover Component"
params:
  seo:
    description: Instructions for setting up base covers in ESPHome.
    image: folder-open.svg
---

The `cover` component is a generic representation of covers in ESPHome.
A cover can (currently) either be *closed* or *open* and supports three types of
commands: *open*, *close* and *stop*.

{{< img src="cover-ui.png" alt="Image" width="75.0%" class="align-center" >}}

{{< anchor "config-cover" >}}

## Base Cover Configuration

All cover config schemas inherit from this schema - you can set these keys for covers.

```yaml
cover:
  - platform: ...
    device_class: garage
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the cover. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the cover to use that name, you can set `name: None`.

- **device_class** (*Optional*, string): The device class for the
  sensor. See <https://www.home-assistant.io/integrations/cover/#device-class> for a list of available options.

- **icon** (*Optional*, icon): Manually set the icon to use for the cover in the frontend.

Advanced options:

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

MQTT options:

- **position_state_topic** (*Optional*, string): The topic to publish
  cover position changes to.

- **position_command_topic** (*Optional*, string): The topic to receive
  cover position commands on.

- **tilt_state_topic** (*Optional*, string): The topic to publish cover
  cover tilt state changes to.

- **tilt_command_topic** (*Optional*, string): The topic to receive
  cover tilt commands on.

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

{{< anchor "cover-open_action" >}}

## `cover.open` Action

This [action](/automations/actions#all-actions) opens the cover with the given ID when executed.

```yaml
on_...:
  then:
    - cover.open: cover_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(cover_1).make_call();
> call.set_command_open();
> call.perform();
> ```

{{< anchor "cover-close_action" >}}

## `cover.close` Action

This [action](/automations/actions#all-actions) closes the cover with the given ID when executed.

```yaml
on_...:
  then:
    - cover.close: cover_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(cover_1).make_call();
> call.set_command_close();
> call.perform();
> ```

{{< anchor "cover-stop_action" >}}

## `cover.stop` Action

This [action](/automations/actions#all-actions) stops the cover with the given ID when executed.

```yaml
on_...:
  then:
    - cover.stop: cover_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(cover_1).make_call();
> call.set_command_stop();
> call.perform();
> ```

{{< anchor "cover-toggle_action" >}}

## `cover.toggle` Action

This [action](/automations/actions#all-actions) toggles the cover with the given ID when executed,
cycling through the states close/stop/open/stop... This allows the cover to be controlled
by a single push button.

```yaml
on_...:
  then:
    - cover.toggle: cover_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(cover_1).make_call();
> call.set_command_toggle();
> call.perform();
> ```

{{< anchor "cover-control_action" >}}

## `cover.control` Action

This [action](/automations/actions#all-actions) is a more generic version of the other cover actions and
allows all cover attributes to be set.

```yaml
on_...:
  then:
    - cover.control:
        id: cover_1
        position: 50%
        tilt: 50%
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The cover to control.
- **stop** (*Optional*, boolean): Whether to stop the cover.
- **state** (*Optional*, string): The state to set the cover to - one of `OPEN` or `CLOSE`.
- **position** (*Optional*, float): The cover position to set.

  - `0.0` = `0%` = `CLOSED`
  - `1.0` = `100%` = `OPEN`

- **tilt** (*Optional*, float): The tilt position to set. In range 0% - 100%.

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(cover_1).make_call();
> // set attributes
> call.set_position(0.5);
> call.perform();
> ```

{{< anchor "cover-lambda_calls" >}}

## Lambdas

From [lambdas](/automations/templates#config-lambda), you can access the current state of the cover (note that these
fields are read-only, if you want to act on the cover, use the `make_call()` method as shown above).

- `position`  : Retrieve the current position of the cover, as a value between `0.0` (closed) and `1.0` (open).

```cpp
        if (id(my_cover).position == COVER_OPEN) {
          // Cover is open
        } else if (id(my_cover).position == COVER_CLOSED) {
          // Cover is closed
        } else {
          // Cover is in-between open and closed
        }
```

- `tilt`  : Retrieve the current tilt position of the cover, as a value between `0.0` and `1.0`.

- `current_operation`  : The operation the cover is currently performing:

```cpp
        if (id(my_cover).current_operation == CoverOperation::COVER_OPERATION_IDLE) {
          // Cover is idle
        } else if (id(my_cover).current_operation == CoverOperation::COVER_OPERATION_OPENING) {
          // Cover is currently opening
        } else if (id(my_cover).current_operation == CoverOperation::COVER_OPERATION_CLOSING) {
          // Cover is currently closing
        }
```

{{< anchor "cover-on_open_trigger" >}}

### `cover.on_open` Trigger

This trigger is activated each time the cover reaches a fully open state.

```yaml
cover:
  - platform: template  # or any other platform
    # ...
    on_open:
      - logger.log: "Cover is Open!"
```

{{< anchor "cover-on_closed_trigger" >}}

### `cover.on_closed` Trigger

This trigger is activated each time the cover reaches a fully closed state.

```yaml
cover:
  - platform: template  # or any other platform
    # ...
    on_closed:
      - logger.log: "Cover is Closed!"
```

## See Also

- {{< apiref "cover/cover.h" "cover/cover.h" >}}
