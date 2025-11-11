---
description: "Animation"
title: "Animation"
---

{{< anchor "display-animation" >}}

Allows to use animated images on displays. Animation inherits all options from the image component.
It adds additional lambda methods: `next_frame()`, `prev_frame()` and `set_frame()` to change the shown picture of a gif.

```yaml
animation:
  - file: "animation.gif"
    id: my_animation
    resize: 100x100
    type: RGB565
```

The animation can be rendered just like the image component with the `image()` function of the display component.

To show the next frame of the animation call `id(my_animation).next_frame()`, to show the previous picture use
`id(my_animation).prev_frame()`. To show a specific picture use `id(my_animation).set_frame(int frame)`.
This can be combined with all Lambdas:

```yaml
display:
  - platform: ...
    # ...
    lambda: |-
      //Ingress shown animation Frame.
      id(my_animation).next_frame();
      // Draw the animation my_animation at position [x=0,y=0]
      it.image(0, 0, id(my_animation), COLOR_ON, COLOR_OFF);
```

Additionally, you can use the `animation.next_frame`, `animation.prev_frame` or `animation.set_frame` actions.

> [!NOTE]
> To draw the next animation independent of Display draw cycle use an interval:
>
> ```yaml
> interval:
>   - interval: 5s
>       then:
>         animation.next_frame: my_animation
> ```

## Configuration variables

- **file** (**Required**, string): The path (relative to where the .yaml file is) of the gif file.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID with which you will be able to reference the animation later
  in your display code.

- **resize** (*Optional*, string): If set, this will resize all the frames to fit inside the given dimensions `WIDTHxHEIGHT`
  and preserve the aspect ratio.

- **type** (**Required**): Specifies how to encode image internally. See the [image component](/components/image#display-image) for
  more information.

  - `BINARY`  : Two colors, suitable for 1 color displays or 2 color image in color displays. Uses 1 bit
    per pixel, 8 pixels per byte. Only `chroma_key` transparency is available.

  - `GRAYSCALE`  : Full scale grey. Uses 8 bits per pixel, 1 pixel per byte.
  - `RGB565`  : Lossy RGB color stored. Uses 2 bytes per pixel, 3 with an alpha channel.
  - `RGB`  : Full RGB color stored. Uses 3 bytes per pixel, 4 with an alpha channel.

- **transparency** (*Optional*): If set the alpha channel of the input image will be taken into account. The possible
  values are `opaque` (default), `chroma_key` and `alpha_channel`. See discussion on transparency in the [image component](/components/image#display-image).
- **loop** (*Optional*): If you want to loop over a subset of your animation (e.g. a fire animation where the fire
  "starts", then "burns" and "dies") you can specify some frames to loop over.

  - **start_frame** (*Optional*, int): The frame to loop back to when `end_frame` is reached. Defaults to the first
    frame in the animation.
  - **end_frame** (*Optional*, int): The last frame to show in the loop; when this frame is reached it will loop back
    to `start_frame`. Defaults to the last frame in the animation.
  - **repeat** (*Optional*, int): Specifies how many times the loop will run. When the count is reached, the animation
    will continue with the next frame after `end_frame`, or restart from the beginning if `end_frame` was the last
    frame. Defaults to "loop forever".

## Actions

- **animation.next_frame**: Moves the animation to the next frame. This is equivalent to the
  `id(my_animation).next_frame();` lambda call.

  - **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the animation to animate.

- **animation.prev_frame**: Moves the animation to the previous frame. This is equivalent to the
  `id(my_animation).prev_frame();` lambda call.

  - **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the animation to animate.

- **animation.set_frame**: Moves the animation to a specific frame. This is equivalent to the
  `id(my_animation).set_frame(frame);` lambda call.

  - **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the animation to animate.
  - **frame** (**Required**, int): The frame index to show next.
