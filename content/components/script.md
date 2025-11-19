---
description: "Script Component"
title: "Script Component"
---

ESPHome's `script` component allows you to define a list of steps (actions) in a central place. You can then execute
the script from nearly anywhere in your device's configuration with a single call.

```yaml
# Example configuration entry
script:
  - id: my_script
    then:
      - switch.turn_on: my_switch
      - delay: 1s
      - switch.turn_off: my_switch
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The [ID](/guides/configuration-types#id) of the script. Use this to interact with the script
  using the script actions.

- **mode** (*Optional*, string): Controls what happens when a script is invoked while it is still running from one or
  more previous invocations. Default to `single`.

  - `single`  : Do not start a new run. Issue a warning.
  - `restart`  : Start a new run after first stopping previous run.
  - `queued`  : Start a new run after previous runs complete. By default, allows up to 5 total instances (1 running + 4 queued).
    When the limit is reached, additional calls are rejected with a warning.
  - `parallel`  : Start a new, independent run in parallel with previous runs.

- **max_runs** (*Optional*, int): Allows limiting the maximum number of script instances.
  - For `queued` mode: Specifies max total instances (including the running one). Defaults to `5` (1 running + 4 queued). Valid range: 1-100.
  - For `parallel` mode: Specifies max parallel instances. Defaults to `0` (unlimited). Valid range: 0-100.

  ```yaml
  script:
    - id: my_script
      mode: queued
      max_runs: 10  # Allow up to 10 total instances (1 running + 9 queued)
      then:
        - logger.log: "Processing..."
  ```

- **parameters** (*Optional*, [Script Parameters](#script-parameters)): A script can define one or more parameters
  that must be provided in order to execute. All parameters defined here are mandatory and must be given when calling
  the script.

- **then** (**Required**, [Action](/automations/actions#all-actions)): The action to perform.

{{< anchor "script-parameters" >}}

## Script Parameters

Scripts can be defined with parameters. The arguments given when calling the script can be used within the script's
lambda actions. To define the parameters, add the parameter names under the `parameters:` key and specify the data
type for that parameter.

Supported data types:

- `bool`  : A boolean true/false. C++ type: `bool`
- `int`  : An integer. C++ type: `int32_t`
- `float`  : A floating point number. C++ type: `float`
- `string`  : A string. C++ type: `std::string`

Each of these also exist in array form:

- `bool[]`  : An array of boolean values. C++ type: `std::vector<bool>`
- Same for other types.

```yaml
script:
  - id: blink_light
    parameters:
      delay_ms: int
    then:
      - light.turn_on: status_light
      # The param delay_ms is accessible using a lambda
      - delay: !lambda return delay_ms;
      - light.turn_off: status_light
```

{{< anchor "script-execute_action" >}}

## `script.execute` Action

This action executes the script. The script **mode** dictates what will happen if the script was already running.

```yaml
# in a trigger:
on_...:
  then:
    - script.execute: my_script

    # Calling a non-parameterised script in a lambda
    - lambda: id(my_script).execute();

    # Calling a script with parameters
    - script.execute:
        id: blink_light
        delay_ms: 500

    # Calling a parameterised script inside a lambda
    - lambda: id(blink_light)->execute(1000);
```

{{< anchor "script-stop_action" >}}

## `script.stop` Action

This action allows you to stop a given script during execution. If the script is not running, it does nothing. This is
useful if you want to stop a script that contains a `delay` action, `wait_until` action, or is inside a `while`
loop, etc. You can also call this action from the script itself, and any subsequent action will not be executed.

```yaml
# Example configuration entry
script:
  - id: my_script
    then:
      - switch.turn_on: my_switch
      - delay: 1s
      - switch.turn_off: my_switch

# in a trigger:
on_...:
  then:
    - script.stop: my_script
```

...or as lambda:

```yaml
lambda: 'id(my_script).stop();'
```

{{< anchor "script-wait_action" >}}

## `script.wait` Action

This action suspends execution of the automation until a script has finished executing.

Note: If no script is executing, this will continue immediately. If multiple instances of the script are running in
parallel, this will block until all of them have terminated.

```yaml
# Example configuration entry
script:
  - id: my_script
    then:
      - switch.turn_on: my_switch
      - delay: 1s
      - switch.turn_off: my_switch

# in a trigger:
on_...:
  then:
    - script.execute: my_script
    - script.wait: my_script
```

This can't be used in a lambda as it would block all functioning of the device. The script wouldn't even get to run.

{{< anchor "script-is_running_condition" >}}

## `script.is_running` Condition

This [condition](/automations/actions#all-conditions) allows you to check if a given script is running. In case scripts are run in
`parallel`, this condition only tells you if at least one script of the given id is running, not how many. Not
designed for use with [while](/automations/actions#while_action); instead try [script.wait](#script-wait_action).

```yaml
on_...:
  if:
    condition:
      - script.is_running: my_script
    then:
      - logger.log: Script is running!
```

...or as lambda:

```yaml
lambda: |-
    if (id(my_script).is_running()) {
        ESP_LOGI("main", "Script is running!");
    }
```

## See Also

- {{< docref "index/" >}}
- {{< docref "/automations/actions" >}}
- {{< docref "/automations/templates" >}}
