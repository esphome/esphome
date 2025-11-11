---
description: "Global Variables"
title: "Global Variables"
---

In some cases you might need to share a global variable across multiple lambdas, or to remember between periods of deep_sleep. For example, global variables can be
used to store the state of a garage door.

```yaml
# Example configuration entry
globals:
  - id: my_global_int
    type: int
    restore_value: no
    initial_value: '0'
  # Example for global string variable
  - id: my_global_string
    type: std::string
    restore_value: yes
    max_restore_data_length: 24
    initial_value: '"Global value is"'

 # In an automation
 on_...:
 then:
   - lambda: |-
       if (id(my_global_int) > 5) {
         // global value is greater than 5
         id(my_global_int) += 1;
       } else {
         id(my_global_int) += 10;
       }

       ESP_LOGD(TAG, "%s: %d", id(my_global_string).c_str(), id(my_global_int));
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): Give the global variable an ID so that you can refer
  to it later in [lambdas](/automations/templates#config-lambda).

- **type** (**Required**, string): The C++ type of the global variable, for example `bool` (for `true`  /`false`  ),
  `int` (for integers), `float` (for decimal numbers), `int[50]` for an array of 50 integers, etc.

- **restore_value** (*Optional*, boolean): Whether to try to restore the state on boot up.
  Be careful: on the ESP8266, you only have a total of 96 bytes available for this! Defaults to `no`.

- **max_restore_data_length** (*Optional*, integer): Only applies to variables of type `std::string`. ESPHome will allocate enough space for this many characters,
  plus single character of overhead. Strings longer than this will not be saved. The max value of this variable is 254 characters, and the default is 63 characters.

- **initial_value** (*Optional*, string): The value with which to initialize this variable if the state
  can not be restored or if state restoration is not enabled. This needs to be wrapped in quotes! Defaults to
  the C++ default value for this type (for example `0` for integers).

Within lambdas globals don’t have a state attribute, just a value. Note in the example above the use of id(my_global_int) where other components would require id(my_component).state.

{{< anchor "globals-set_action" >}}

## `globals.set` Action

This [Action](/automations/actions#all-actions) allows you to change the value of a `global`
variable without having to use the lambda syntax.

```yaml
on_...:
  - globals.set:
      id: my_global_var
      value: '10'
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The [ID](/guides/configuration-types#id) of the global variable to set.
- **value** (**Required**, [templatable](/automations/templates)): The value to set the global
  variable to.

> [!NOTE]
> This action can also be written in lambdas:
>
> ```cpp
> id(my_global_var) = 10;
> ```

## See Also

- {{< docref "index/" >}}
- {{< docref "/automations/actions" >}}
- {{< docref "/automations/templates" >}}
