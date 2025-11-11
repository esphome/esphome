---
description: "Guide for using templates in ESPHome"
title: "Templates"
params:
  seo:
    description: Guide for using templates in ESPHome
    image: auto-fix.svg
---

{{< anchor "config-lambda" >}}

*Templates* (also known as *lambdas*) allow you to do almost *anything* in ESPHome. For example, if you want to only
perform a certain automation if a certain complex formula evaluates to true, you can do that with templates. Let's look
at an example first:

```yaml
binary_sensor:
  - platform: gpio
    name: "Cover End Stop"
    id: top_end_stop
cover:
  - platform: template
    name: Living Room Cover
    lambda: !lambda |-
      if (id(top_end_stop).state) {
        return COVER_OPEN;
      } else {
        return COVER_CLOSED;
      }
```

What's happening here? First, we define a binary sensor (notably with `id: top_end_stop`  ) and then a
{{< docref "/components/cover/template" "template cover" >}}. (If you're new to Home Assistant, a 'cover' is something
like a window blind, a roller shutter, or a garage door.) The *state* of the template cover is controlled by a template,
or "lambda". In lambdas, you're just writing C++ code and therefore the name lambda is used instead of Home Assistant's
"template" lingo to avoid confusion. Regardless, don't let lambdas scare you just because you saw "C++" -- writing
lambdas is not that hard! Here's a bit of a primer:

First, you might have already wondered what the `lambda: !lambda |-` part is supposed to mean. `!lambda` tells
ESPHome that the following block is supposed to be interpreted as a lambda, or C++ code. Note that here, the
`lambda:` key would actually implicitly make the following block a lambda, so in this context, you could also have
written `lambda: |-`.

Next, there's the weird `|-` character combination. This tells the YAML parser to treat the following **indented**
block as plaintext. Without it, the YAML parser would attempt to read the following block as if it were made up of YAML
keys like `cover:` for example. (You may also have seen variations of this like `>-` or just `|` or `>`. There
is a slight difference in how these different styles deal with whitespace, but for our purposes we can ignore that).

With `if (...) { ... } else { ... }` we create a *condition*. What this effectively says that if the thing inside the
first parentheses evaluates to `true` then execute the first block (in this case `return COVER_OPEN;`, or else
evaluate the second block. `return ...;` makes the code block give back a value to the template. In this case, we're
either *returning* `COVER_OPEN` or `COVER_CLOSED` to indicate that the cover is closed or open.

Finally, `id(...)` is a helper function that makes ESPHome fetch an object with the supplied ID (which you defined
somewhere else, like `top_end_stop`  ) and lets you call any of ESPHome's many APIs directly. For example, here we're
retrieving the current state of the end stop using `.state` and using it to construct our cover state.

> [!NOTE]
> ESPHome does not check the validity of lambda expressions you enter and will blindly copy them into the generated
> C++ code. If compilation fails or something else is not working as expected with lambdas, it's always best to look
> at the generated C++ source file under `<NODE_NAME>/src/main.cpp`.

> [!TIP]
> To store local variables inside lambdas that retain their value across executions, you can create `static`
> variables as shown in the example below. Here, the variable `num_executions` is incremented by one each time the
> lambda is executed and the current value is logged.
>
> ```yaml
> lambda: |-
>   static int num_executions = 0;
>   ESP_LOGD("main", "I am at execution number %d", num_executions);
>   num_executions += 1;
> ```

{{< anchor "config-templatable" >}}

## Templating Actions

ESPHome allows you to template most parameters for actions used in automations. For example, if you have a light and
want to set it to a pre-defined color when a button is pressed, you can do this:

```yaml
on_press:
  then:
    - light.turn_on:
        id: some_light_id
        transition_length: 0.5s
        red: 0.8
        green: 1.0
        blue: !lambda |-
          // The sensor outputs values from 0 to 100. The blue
          // part of the light color will be determined by the sensor value.
          return id(some_sensor).state / 100.0;
```

When you see the label "templatable" in the documentation for a given action, it can be templated as in this example,
using the lambda syntax as described/shown above.

## All Lambda Calls

- [Sensor](/components/sensor#sensor-lambda_calls)
- [Binary Sensor](/components/binary_sensor#binary_sensor-lambda_calls)
- [Switch](/components/switch#switch-lambda_calls)
- [Display](/components/display#display-engine)
- [Cover](/components/cover#cover-lambda_calls)
- [Text Sensor](/components/text_sensor#text_sensor-lambda_calls)
- [Stepper](/components/stepper#stepper-lambda_calls)
- [Number](/components/number#number-lambda_calls)

## See Also

- {{< docref "index/" >}}
- {{< docref "actions/" >}}
