---
description: "Instructions for setting up event components in ESPHome."
title: "Event Component"
params:
  seo:
    description: Instructions for setting up event components in ESPHome.
    image: folder-open.svg
---

ESPHome supports the creation of event entities in Home Assistant.
These entities allow for the triggering of custom events within the Home Assistant ecosystem,
enabling complex automations and integrations. An event entity is represented as a stateless
entity associated with a device that has a pre-defined set of event types which can be
triggered in Home Assistant via automations.

> [!NOTE]
> Events in ESPHome are designed to trigger an action in Home Assistant, and have a unidirectional flow from ESPHome to Home Assistant.
> Home Assistant event entities are different from events on event bus. If you just want to trigger an event on the
> Home Assistant event bus, you should use a [Home Assistant event](/components/api#api-homeassistant_event_action) instead.

> [!NOTE]
> Home Assistant Core 2024.5 or higher is required for ESPHome event entities to work.

{{< anchor "config-event" >}}

## Base Event Configuration

Each event in ESPHome needs to be configured with a list of event types it can trigger and an optional device class.

```yaml
# Example event configuration
event:
  - platform: ...
    name: Motion Detected Event
    id: my_event

    # Optional variables:
    icon: "mdi:motion-sensor"
    device_class: "motion"
    on_event:
      then:
        - logger.log: "Event triggered"
```

Configuration variables:

One of `id` or `name` is required.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the event. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the event to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the event in the frontend.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options. Set to `""` to remove the default entity category.

- **device_class** (*Optional*, string): The device class for the event. The following device classes are supported by event entities:

  - None: Generic event. This is the default and doesn't need to be set.
  - `button`  : For remote control buttons.
  - `doorbell`  : Specifically for buttons that are used as a doorbell.
  - `motion`  : For motion events detected by a motion sensor.

  See <https://www.home-assistant.io/integrations/event/#device-class>
  for a list of available options.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

Automations:

- **on_event** (*Optional*, [Automation](/automations)): An automation to perform when an event is triggered.

MQTT options:

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

## Event Automation

{{< anchor "event-on_event" >}}

### `on_event`

This automation will be triggered when an event of the specified types is triggered.
In [Lambdas](/automations/templates#config-lambda) you can get the event type from the trigger with `event_type`.

```yaml
event:
  - platform: template
    # ...
    on_event:
      then:
        - lambda: |-
            ESP_LOGD("main", "Event %s triggered.", event_type.c_str());
```

Configuration variables: see [Automation](/automations).

### `event.trigger` Action

This action allows for the triggering of an event from within an automation.

```yaml
- event.trigger:
    id: my_event
    event_type: "custom_event"
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the event.
- **event_type** (**Required**, string): The type of event to trigger.

{{< anchor "event-lambda_calls" >}}

### lambda Calls

From [lambdas](/automations/templates#config-lambda), you can trigger an event.

- `trigger(std::string event_type)`  : Trigger an event with the specified type.

```cpp
    // Within lambda, trigger the event.
    id(my_event).trigger("custom_event");
```

## See Also

- {{< apiref "event/event.h" "event/event.h" >}}
