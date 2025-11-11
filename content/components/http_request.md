---
description: "Instructions for setting up HTTP Requests in ESPHome"
title: "HTTP Request"
params:
  seo:
    description: Instructions for setting up HTTP Requests in ESPHome
    image: connection.svg
---

The `http_request` component lets you make HTTP/HTTPS requests. To do so, you need to add it to your device's configuration:

```yaml
# Example configuration entry
http_request:
```

{{< anchor "http_request-configuration_variables" >}}

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **follow_redirects** (*Optional*, boolean): Enable following HTTP redirects. Defaults to `true`.
- **redirect_limit** (*Optional*, integer): Maximum amount of redirects to follow when enabled. Defaults to `3`.
- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): Timeout for request. Defaults to `4.5s`.
- **useragent** (*Optional*, string): User-Agent header for requests. Defaults to
  `ESPHome/<version> (https://esphome.io)` where `<version>` is the version of ESPHome the device is running.
  For example: `ESPHome/2024.6.0 (https://esphome.io)`

- **verify_ssl** (*Optional*, boolean): When set to `true` (default), SSL/TLS certificates will be validated upon
  connection; if invalid, the connection will be aborted. To accomplish this, ESP-IDF's default ESP x509 certificate
  bundle is included in the build. This certificate bundle includes the complete list of root certificates from
  Mozilla's NSS root certificate store. **May only be set to true when using the ESP-IDF framework; must be explicitly
  set to false when using the Arduino framework.**

- **watchdog_timeout** (*Optional*, [Time](/guides/configuration-types#time)): Change the watchdog timeout during connection/data transfer.
  May be useful on slow connections or connections with high latency. **Do not change this value unless you are
  experiencing device reboots due to watchdog timeouts;** doing so may prevent the device from rebooting due to a
  legitimate problem. **Only available on ESP32 and RP2040**.

**For the ESP32 when using ESP-IDF:**

- **buffer_size_rx** (*Optional*, integer): Change HTTP receive buffer size. Defaults to `512`.
- **buffer_size_tx** (*Optional*, integer): Change HTTP transmit buffer size. Defaults to `512`.

**For the ESP8266:**

- **esp8266_disable_ssl_support** (*Optional*, boolean): Determines whether to include HTTPS/SSL support in the
  firmware binary. Excluding the SSL libraries from your build will result in a smaller binary, which may be
  necessary for memory-constrained devices (512 kB or 1 MB). If you see
  `Error: ESP does not have enough space to store OTA file` in your device's logs, you may need to enable this
  option. Defaults to `false`. By setting this option to `true`  :

  - HTTPS connections will not be possible
  - `verify_ssl: false` is implied

> [!WARNING]
> Setting `verify_ssl` to `false` **reduces security** when using HTTPS connections!
>
> Without the root certificate bundle, certificates used by the remote HTTPS server cannot be verified, opening the
> HTTPS connection up to person-in-the-middle attacks.
>
> To maximize security, do not set `verify_ssl` to `false` *unless:*
>
> - a custom CA/self-signed certificate is used,
> - the Arduino framework is used, or
> - the device does not have sufficient memory to store the certificate bundle
>
> **We strongly recommend using hardware which properly supports TLS/SSL.**

**For the host platform:**

- **ca_certificate_path** (*Optional*, file path): Path to a CA certificate bundle. Not required on MacOS (the inbuilt CA bundle is used and SSL enabled by default).
   On Linux this is required to enable SSL.

> [!NOTE]
> To use SSL on Linux you must have the `libssl-dev` package installed (e.g. `sudo apt install libssl-dev`  ).
> A typical value on Linux for `ca_certificate_path` would be `/etc/ssl/certs/ca-certificates.crt`.

## Actions

The `http_request` component supports a number of [actions](/automations/actions#all-actions) that can be used to send requests.

{{< anchor "http_request-get_action" >}}

### `http_request.get` Action

This [action](/automations/actions#all-actions) sends a GET request.

```yaml
on_...:
  - http_request.get:
      url: https://esphome.io
      request_headers:
        Content-Type: application/json
      on_response:
        then:
          - logger.log:
              format: 'Response status: %d, Duration: %u ms'
              args:
                - response->status_code
                - response->duration_ms
  # Short form
  - http_request.get: https://esphome.io
```

#### Configuration variables

- **url** (**Required**, string, [templatable](/automations/templates)): URL to which to send the request.
- **request_headers** (*Optional*, mapping): Map of HTTP headers. Values are [templatable](/automations/templates).
- **collect_headers** (*Optional*, list of strings): List of the names of HTTP headers to collect from the response.
- **capture_response** (*Optional*, boolean): when set to `true`, the response data will be captured and placed into
  the `body` variable as a `std::string` for use in [lambdas](/automations/templates#config-lambda). Defaults to `false`.

- **max_response_buffer_size** (*Optional*, integer): The maximum buffer size to be used to store the response.
  Defaults to `1 kB`.

- **on_response** (*Optional*, [Automation](/automations)): An automation to perform after the request is received.
- **on_error** (*Optional*, [Automation](/automations)): An automation to perform if the request cannot be completed.

{{< anchor "http_request-post_action" >}}

### `http_request.post` Action

This [action](/automations/actions#all-actions) sends a POST request.

```yaml
on_...:
  - http_request.post:
      url: https://esphome.io
      request_headers:
        Content-Type: application/json
      json:
        key: value
  # Short form
  - http_request.post: https://esphome.io
```

#### Configuration variables

- **body** (*Optional*, string, [templatable](/automations/templates)): A HTTP body string to send with request.
- **json** (*Optional*, mapping): A HTTP body in JSON format. Values are [templatable](/automations/templates).
  See [Examples](#http_request-examples).

- All other options from [`http_request.get` Action](#http_request-get_action).

{{< anchor "http_request-send_action" >}}

### `http_request.send` Action

This [action](/automations/actions#all-actions) sends a request.

```yaml
on_...:
  - http_request.send:
      method: PUT
      url: https://esphome.io
      request_headers:
        Content-Type: application/json
      body: "Some data"
```

#### Configuration variables

- **method** (**Required**, string): HTTP method to use (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`  ).
- All other options from [`http_request.post` Action](#http_request-post_action) and [`http_request.get` Action](#http_request-get_action).

## Triggers

{{< anchor "http_request-on_response" >}}

### `on_response` Trigger

This automation will be triggered when the HTTP request is complete.
The following variables are available for use in [lambdas](/automations/templates#config-lambda):

- `response` as a pointer to `HttpContainer` object which contains `content_length`, `status_code` and `  duration_ms``.
- `std::string get_response_header(const std::string &header_name)` to read response headers (only headers with names specified in the `collect_headers` are available).
- `body` as `std::string` which contains the response body when `capture_response`
  (see [`http_request.get` Action](#http_request-get_action)) is set to `true`.

> [!NOTE]
> The `status_code` should be checked before using the `body` variable. A successful response will usually have
> a status code of `200`. Server errors such as "not found" (404) or "internal server error" (500) will have an appropriate status code, and may contain an error message in the `body` variable.

```yaml
on_...
  then:
    - http_request.get:
        url: https://esphome.io
        collect_headers:
          - Content-Type
        on_response:
          then:
            - logger.log:
                format: "Response status: %d, Duration: %u ms, Content-Type: %s"
                args:
                  - response->status_code
                  - response->duration_ms
                  - response->get_response_header("Content-Type").c_str()
            - lambda: |-
                ESP_LOGD(TAG, "Response status: %d, Duration: %u ms, Content-Type: %s", response->status_code, response->duration_ms, response->get_response_header("Content-Type").c_str());
        on_error:
          then:
            - logger.log: "Request failed!"
```

{{< anchor "http_request-on_error" >}}

### `on_error` Trigger

This automation will be triggered when the HTTP request fails to complete. This may be e.g. when the network is not available,
or the server is not reachable. This will *not* be triggered if the request
completes, even if the response code is not 200. No information on the type of error is available and no variables
are available for use in [lambdas](/automations/templates#config-lambda). See example usage above.

{{< anchor "http_request-examples" >}}

## Examples

### Templatable values

```yaml
on_...:
  - http_request.post:
      url: !lambda |-
        return ((std::string) "https://esphome.io?state=" + id(my_sensor).state).c_str();
      request_headers:
        X-Custom-Header: !lambda |-
          return ((std::string) "Value-" + id(my_sensor).state).c_str();
      body: !lambda |-
        return id(my_sensor).state;
```

### POST Body in JSON format (syntax 1)

**Note:** all values of the map must be strings. It is not possible to send JSON `boolean` or `numbers` with this
syntax.

```yaml
on_...:
  - http_request.post:
      url: https://esphome.io
      json:
        key: !lambda |-
          return id(my_sensor).state;
        greeting: "Hello World"

    # Will send:
    # {"key": "42.0", "greeting": "Hello World"}
```

### POST Body in JSON format (syntax 2)

**Note:** use this syntax to send `boolean` or `numbers` in JSON.

The JSON message will be constructed using the [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library.
In the `json` option you have access to a `root` object which represents the base object of the JSON message. You
can assign values to keys by using the `root["KEY_NAME"] = VALUE;` syntax as shown below.

```yaml
on_...:
  - http_request.post:
      url: https://esphome.io
      json: |-
        root["key"] = id(my_sensor).state;
        root["greeting"] = "Hello World";

    # Will send:
    # {"key": 42.0, "greeting": "Hello World"}
```

### GET values from a JSON body response

If you want to retrieve the value for the vol key and assign it to a template sensor or number component whose id is
set to player_volume you can do this, but note that checking for the presence of the key will prevent difficult-to-read
error messages:

This example assumes that the server returns a response as a JSON object similar to this:
`{"status":"play","vol":"42","mute":"0"}`

If you want to retrieve the value for the `vol` key and assign it to a template `sensor` or `number` component
whose `id` is set to `player_volume`  :

```yaml
on_...:
- http_request.get:
    url: https://esphome.io
    capture_response: true
    on_response:
      then:
        - if:
            condition:
                lambda: return response->status_code == 200;
            then:
                - lambda: |-
                    json::parse_json(body, [](JsonObject root) -> bool {
                        if (root["vol"]) {
                            id(player_volume).publish_state(root["vol"]);
                            return true;
                        }
                        else {
                          ESP_LOGI(TAG,"No 'vol' key in this json!");
                          return false;
                        }
                    });
            else:
                - logger.log:
                    format: "Error: Response status: %d, message %s"
                    args: [ 'response->status_code', 'body.c_str()' ]
```

## See Also

- {{< docref "index/" >}}
- {{< apiref "http_request/http_request.h" "http_request/http_request.h" >}}
- {{< docref "/components/json" >}}
