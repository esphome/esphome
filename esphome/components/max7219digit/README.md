https://user-images.githubusercontent.com/10338724/81001535-2c491d00-8e48-11ea-86f2-2b0bb60eacb0.jpeg

```yaml
display:
  - platform: max7219digit
    cs_pin: D8
    num_chips: 4
    lambda: |-
      it.strftime(0, 0, id(digit_font), "%H:%M", id(hass_time).now());
      it.image(24, 0, id(my_image));

font:
  - file: "pixelmix.ttf"
    id: digit_font
    size: 8
  - file: "pixelmix_bold.ttf"
    id: digitbolt_font
    size: 6

time:
  - platform: homeassistant
    id: hass_time

image:
  - file: "smile.png"
    id: my_image
```

So if you want you can put 10 in a row. Just adjust the number of chips. But I want to give a parameter how they are stacked. As you can also place them on top of each other.


```yaml
display:
  - platform: max7219digit
    cs_pin: D8
    num_chips: 4
    offset: 2
    intensity: 15
    lambda: |-
      it.strftime(0, 0, id(digit_font), "%H:%M%a", id(hass_time).now());
      it.scroll_left(1);
```
offset = number of extra "virtual chips to be displayed with scroll"
it.scroll_left(X): Move the content of display every update X steps to the left and add to end of the buffer to create a continuous loop.

```yaml
display:
  - platform: max7219digit
    cs_pin: D8
    num_chips: 4
    offset: 2
    intensity: 15
    lambda: |-
      it.strftime(0, 0, id(digit_font), "%H:%M%a", id(hass_time).now());
      it.scroll_left(1);
      it.invert_on_off();
```
it.invert_on_off(); -> Inverts the output of the display Background and Dots the next time update is run. To be put at the end of the lambda input.
it.invert_on_off(true); -> Output is inverted
it.invert_on_off(false);-> Output is normal

The invert can also be set in between the lambda call to set the dot color to write.
```yaml
it.invert_on_off();
it.line(X,Y,X,Y);
it.invert_on_off();
```
Wipes a line of text in the display. Can be used to blink!
