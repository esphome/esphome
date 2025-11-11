---
description: "Instructions for setting up generic locks in ESPHome."
title: "Lock Component"
params:
  seo:
    description: Instructions for setting up generic locks in ESPHome.
    image: folder-open.svg
---

The `lock` domain includes all platforms that should function like a lock
with lock/unlock actions.

{{< anchor "config-lock" >}}

## Base Lock Configuration

```yaml
lock:
  - platform: ...
    name: "Lock Name"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name of the lock. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the lock to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the
  lock in the frontend.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **on_lock** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the lock is locked. See [`lock.on_lock` / `lock.on_unlock` Trigger](#lock-on_lock_unlock_trigger).

- **on_unlock** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the lock is unlocked. See [`lock.on_lock` / `lock.on_unlock` Trigger](#lock-on_lock_unlock_trigger)..

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options. Set to `""` to remove the default entity category.

- If MQTT enabled, All other options from [MQTT Component](/components/mqtt#config-mqtt-component).
- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

{{< anchor "lock-lock_action" >}}

### `lock.lock` Action

This action locks a lock with the given ID on when executed.

```yaml
on_...:
  then:
    - lock.lock: deadbolt_1
```

{{< anchor "lock-unlock_action" >}}

### `lock.unlock` Action

This action unlocks a lock with the given ID off when executed.

```yaml
on_...:
  then:
    - lock.unlock: deadbolt_1
```

{{< anchor "lock-open_action" >}}

### `lock.open` Action

This action opens (e.g. unlatch) a lock with the given ID off when executed.

```yaml
on_...:
  then:
    - lock.open: doorlock_1
```

{{< anchor "lock-is_locked_condition" >}}
{{< anchor "lock-is_unlocked_condition" >}}

### `lock.is_locked` / `lock.is_unlocked` Condition

This [Condition](/automations/actions#all-conditions) checks if the given lock is LOCKED (or UNLOCKED).

```yaml
# In some trigger:
on_...:
  if:
    condition:
      # Same syntax for is_unlocked
      lock.is_locked: my_lock
```

{{< anchor "lock-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all locks to do some
advanced stuff (see the full API Reference for more info).

- `publish_state()`  : Manually cause the lock to publish a new state and store it internally.
  If it's different from the last internal state, it's additionally published to the frontend.

```yaml
    // Within lambda, make the lock report a specific state
    id(my_lock).publish_state(LOCK_STATE_LOCKED);
    id(my_lock).publish_state(LOCK_STATE_UNLOCKED);
```

- `state`  : Retrieve the current state of the lock.

```yaml
    // Within lambda, get the lock state and conditionally do something
    if (id(my_lock).state == LOCK_STATE_LOCKED) {
      // Lock is LOCKED, do something here
    }
```

- `unlock()`  /`lock()`  /`open()`  : Manually lock/unlock/open a lock from code.
  Similar to the `lock.lock`, `lock.unlock`, and `lock.open` actions,
  but can be used in complex lambda expressions.

```yaml
    id(my_lock).unlock();
    id(my_lock).lock();
    id(my_lock).open();
```

{{< anchor "lock-on_lock_unlock_trigger" >}}

### `lock.on_lock` / `lock.on_unlock` Trigger

This trigger is activated each time the lock is locked/unlocked. It becomes active
right after the lock component has acknowledged the state (e.g. after it LOCKED/UNLOCKED itself).

```yaml
lock:
  - platform: template  # or any other platform
    # ...
    on_lock:
    - logger.log: "Door Locked!"
    on_unlock:
    - logger.log: "Door Unlocked!"
```

## See Also

- {{< apiref "lock/lock.h" "lock/lock.h" >}}
