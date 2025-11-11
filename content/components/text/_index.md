---
description: "Instructions for setting up text components in ESPHome."
title: "Text Component"
params:
  seo:
    description: Instructions for setting up text components in ESPHome.
    image: folder-open.svg
---

ESPHome has support for components to create a text entity. A text entity is
like a `text_sensor` that can read a value from a device, but is useful when that value
can be set by the user/frontend.

> [!NOTE]
> Home Assistant Core 2023.11 or higher is required for ESPHome text entities to work.

{{< anchor "config-text" >}}

## Base Text Configuration

All texts in ESPHome have a name and an optional icon.

```yaml
# Example text configuration
name: Livingroom Text

# Optional variables:
icon: "mdi:cursor-text"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (**Required**, string): The name for the text.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the text to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the text in the frontend.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options. Set to `""` to remove the default entity category.

- **mode** (**Required**, string): Defines how the text should be displayed in the frontend.
  One of `text` or `password`.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

Automations:

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when a new value is published. See [`on_value`](#text-on_value).

MQTT Options:

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

## Text Automation

You can access the most recent state of the text in [lambdas](/automations/templates#config-lambda) using
`id(text_id).state`.

{{< anchor "text-on_value" >}}

### `on_value`

This automation will be triggered when a new value is published. In [Lambdas](/automations/templates#config-lambda)
you can get the value from the trigger with `x`.

```yaml
text:
  - platform: template
    # ...
    on_value:
      then:
        - logger.log:
            format: "%s"
            args: ["x.c_str()"]
```

Configuration variables: See [Automation](/automations).

{{< anchor "text-set_action" >}}

### `text.set` Action

This is an [Action](/automations/actions#all-actions) for setting a text state.

```yaml
- text.set:
    id: my_text
    value: "Hello World"
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the text to set.
- **value** (**Required**, string, [templatable](/automations/templates)):
  The value to set the text to.

{{< anchor "text-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call certain methods on all texts to do some
advanced stuff (see the full API Reference for more info).

- `.make_call()`  : Make a call for updating the text value.

```cpp
    // Within lambda, push a value of 42
    auto call = id(my_text).make_call();
    call.set_value("Hello World");
    call.perform();
```

- `.state`  : Retrieve the current value of the text.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my text: %s", id(my_text).state.c_str());
```

## See Also

- {{< apiref "Text" "text/text.h" >}}
- {{< apiref "TextCall" "text/text_call.h" >}}
