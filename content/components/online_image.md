---
description: "Instructions for displaying images downloaded at runtime in ESPHome."
title: "Online Image Component"
params:
  seo:
    description: Instructions for displaying images downloaded at runtime in ESPHome.
    image: image-sync-outline.svg
---

{{< anchor "online_image" >}}

With this component you can define images that will be downloaded, decoded and drawn at runtime.

> [!NOTE]
> Current supported formats:
>
> - BMP images
>
>   - 1-bit / binary / black and white
>   - 24-bit / RGB
>
> - JPEG images, currently only baseline images (no progressive support)
>
> - PNG images

> [!WARNING]
> This component requires a fair amount of RAM; both for downloading the image, and for storing the decoded image. It might work on devices without PSRAM, but there is no guarantee.

This component has a dependency to {{< docref "/components/http_request" >}}; the configuration options you set to the `http_request` component will also apply here.

```yaml
online_image:
  - url: "https://example.com/example.png"
    format: png
    id: my_online_image
```

## Configuration variables

- **url** (**Required**, url): The URL where the image will be downloaded from.
- **request_headers** (*Optional*, mapping): Map of HTTP headers. Values are [templatable](/automations/templates).
- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID with which you will be able to reference the image later
  in your display code.

- **format** (**Required**): The format that the image is encoded with.

  - `BMP`  : The image on the server is encoded in BMP format.
  - `JPEG`  : The image on the server is encoded in JPEG format.
  - `PNG`  : The image on the server is encoded in PNG format.
- **resize** (*Optional*, string): If set, this will resize the image to fit inside the given dimensions `WIDTHxHEIGHT`
  and preserve the aspect ratio.

- **placeholder** (*Optional*, [ID](/guides/configuration-types#id)): ID of an {{< docref "/components/image" "Image" >}} to display while the downloaded image is not yet ready.
  This placeholder image will **not** be resized; regardless of the `resize` option value for the `online_image`.

- **type** (*Required*): Specifies how to encode image internally.

  - `BINARY`  : Two colors, suitable for 1 color displays or 2 color image in color displays. Uses 1 bit
    per pixel, 8 pixels per byte. Only `chroma_key` transparency is available.

  - `GRAYSCALE`  : Full scale grey. Uses 8 bits per pixel, 1 pixel per byte.
  - `RGB565`  : Lossy RGB color stored. Uses 2 bytes per pixel, 3 with an alpha channel
  - `RGB`  : Full RGB color stored. Uses 3 bytes per pixel, 4 with an alpha channel.
- **transparency** (*Optional*): If set the alpha channel of the input image will be taken into account. The possible values are `opaque` (default), `chroma_key` and `alpha_channel`. See the discussion on transparency in the [image component](/components/image#display-image).
- **byte_order** (*Optional*, string): For RGB565 images, the pixels are converted to 16 bit values. By default these will be stored in big endian byte order (MSB first),
  but you can override this by setting `byte_order` to `little_endian`. Options are `big_endian` (default) and `little_endian`.
  Not applicable to other image formats.

- **update_interval** (*Optional*, int): Redownload the image when the specified time has elapsed. Defaults to `never` (i.e. the update component action needs to be called manually).

Advanced options:

- **buffer_size** (*Optional*, int): Explicitly specify the size of the buffer where the image chunks are being downloaded while decoding. The default value (65536) should be OK for most use cases, but you can try to reduce the size for slow connections, to avoid watchdog timeouts.

## Automations

- **on_download_finished** (*Optional*, [Automation](/automations)): An automation to perform when the image has been successfully downloaded.

The variable `cached` is a boolean available in [lambdas](/automations/templates#config-lambda) that indicates cache status:

- `true` if the image was loaded from cache (cache hit).
- `false` if the image was freshly downloaded (cache miss).

Caching follows standard HTTP mechanisms (see [HTTP caching](https://developer.mozilla.org/en-US/docs/Web/HTTP/Caching)), utilizing the `Last-Modified` and `ETag` headers.

For example:

```yaml
online_image:
  - url: "https://upload.wikimedia.org/wikipedia/commons/thumb/4/47/PNG_transparency_demonstration_1.png/280px-PNG_transparency_demonstration_1.png"
    format: png
    id: my_online_image
    on_download_finished:
      lambda: |-
        if (cached) {
          ESP_LOGD("online_image", "Cache hit: using cached image");
        } else {
          ESP_LOGD("online_image", "Cache miss: fresh download");
        }
```

A good example for that is to update the display component after the download succeeded.

- **on_error** (*Optional*, [Automation](/automations)): An automation to perform when an error happened during download or decode.

## Actions

### `online_image.set_url` Action

Change the URL where the image is downloaded from. A re-download will be automatically triggered unless `update` is set to `false`.

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The image to update the URL for.
- **url** (**Required**, url): The new URL to download the image from.
- **update** (*Optional*, bool): If `true`, the image will be updated (fetched) immediately after setting the new URL. If `false`, the URL will be set but the image will **not** be updated until you call the `update` action. Defaults to `true`

```yaml
on_...:
  - online_image.set_url:
      id: my_online_image
      url: "https://www.example.com/new_image.png"
  - component.update: my_online_image
```

### `online_image.release` Action

Release the memory currently used by an image. Can be used if different display pages need different images, to avoid wasting memory on an image that is currently not being displayed.

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The image to update the URL for.

```yaml
on_...:
  - online_image.release: my_online_image
```

## Examples

```yaml
online_image:
  - url: "https://upload.wikimedia.org/wikipedia/commons/thumb/4/47/PNG_transparency_demonstration_1.png/280px-PNG_transparency_demonstration_1.png"
    format: png
    id: my_online_image
    on_download_finished:
      component.update: my_display
```

And then later in code:

```yaml
display:
  - platform: ...
    id: my_display
    # ...
    lambda: |-
      // Draw the image my_online_image at position [x=0,y=0]
      it.image(0, 0, id(my_online_image));
```

For monochrome displays the `image` method accepts two additional color parameters which can
be supplied to specify the color used to draw bright and dark pixels respectively.
In this case the image will be internally converted to a grayscale image and then to monochrome
based on an internally defined threshold.

```yaml
display:
  - platform: ...
    id: my_display
    # ...
    lambda: |-
      // Draw the image my_image at position [x=0,y=0]
      // with front color "OFF" and back color "ON"
      it.image(0, 0, id(my_online_image), COLOR_OFF, COLOR_ON);
```

By default `online_image` is configured to not automatically update/download the image; in order to do the initial download, you can either:

- Add a `component.update <image_id>` in the `on_connect:` action on the {{< docref "/components/wifi" >}} component.
- Explicitly set an `update_interval`.
- Call `component.update <image_id>` in an {{< docref "/components/interval" >}} block.
- Call `component.update <image_id>` where you need the image to be downloaded/updated.

```yaml
wifi:
  on_connect:
    - component.update: my_online_image
```

## See Also

- {{< apiref "online_image/online_image.h" "online_image/online_image.h" >}}
- {{< docref "image" "Image Component" >}}
- {{< docref "animation" "Animation Component" >}}
