---
description: "Instructions for parsing and building json within ESPHome."
title: "json Component"
params:
  seo:
    description: Instructions for parsing and building json within ESPHome.
---

The `json` component enables ESPHome to work with JSON data in automations, sensors, and HTTP requests. This is particularly useful for:

- Processing API responses
- Sending structured data to external services
- Parsing configuration from JSON files

What is JSON?

JSON is a text syntax that facilitates structured data interchange between all programming languages. JSON
is a syntax of braces, brackets, colons, and commas that is useful in many contexts, profiles, and applications.
JSON stands for JavaScript Object Notation and was inspired by the object literals of JavaScript aka
ECMAScript as defined in the [ECMAScript Language Specification, Third Edition](https://ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf).

Example 1: Relatively complex JSON

```json
{
 "first_name": "Chad",
 "last_name": "Matsalla",
 "is_alive": true,
 "age": 27,
 "address": {
   "street_address": "42 Nowhere Street",
   "city": "Fort Qu'Appelle",
   "state": "Saskatchewan",
   "postal_code": "S0G 1S0",
   "country": "Canada"
 },
 "phone_numbers": [
   {
     "type": "home",
     "number": "212 555-1234"
   },
   {
     "type": "office",
     "number": "646 555-4567"
   }
 ],
 "children": [
   "Austin",
   "Alex",
   "Juno",
   "Beethoven",
 ],
 "spouse": "Sarah"
}
```

Example 2: Simple JSON:

```json
{"key": 42.0, "greeting": "Hello World"}
```

> [!NOTE]
> To use the json component, you need to include it in your config. Be sure to put `json:` at the root level, along with other components like `esphome:`.

## Parsing JSON

This example assumes that the server returns a response as a JSON object similar to this:
`{"status":"play","vol":"42","mute":"0"}`

If you want to retrieve the value for the `vol` key and assign it to a template `sensor` or `number` component
whose `id` is set to `player_volume` you can do this, but note that checking for the presence of the key will prevent difficult-to-read error messages:

```yaml
on_...:
- http_request.get:
    url: https://esphome.io
    capture_response: true
    on_response:
      then:
        - lambda: |-
            json::parse_json(body, [](JsonObject root) -> bool {
                if (root["vol"]) {
                    id(player_volume).publish_state(root["vol"]);
                    return true;
                }
                else {
                  ESP_LOGD(TAG,"No 'vol' key in this json!");
                  return false;
                }
            });
```

## Building JSON

You can build JSON in a lambda with a nested array like this:

```yaml
on_...:
  - http_request.post:
      url: https://esphome.io
      json: |-
        root["key"] = id(my_sensor).state;
        root["greeting"] = "Hello World";
```

This will send::
 `{"key": 42.0, "greeting": "Hello World"}`

## Troubleshooting Errors

A very common error when deserializing is:

```text
JSON parse error: InvalidInput
```

The software ESPHome uses does not provide particularly informative messages as to why, but
the people at ArduinoJson have created a [wonderful troubleshooter](https://arduinojson.org/troubleshooter).

Another important resource is [JSONLint](https://jsonlint.com/). It will help you determine if the JSON you are using is valid. It must be valid to work with ESPHome's deserializer and it probably needs to be valid for the destination, if you are sending it.

## See Also

- {{< docref "index/" >}}
- {{< apiref "HTTP Request" "http_request/http_request.h" >}}
- {{< apiref "json_util.h" "json/json_util.h" >}}
- [ArduinoJson](https://arduinojson.org/)
