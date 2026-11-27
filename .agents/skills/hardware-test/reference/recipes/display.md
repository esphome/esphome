# display

## Recording fake display for image and color changes (#23229, #23226)

No panel needed: `scripts/display_recorder/components/recdisp/` is a 64x64 `display` platform that stores every
`draw_pixel_at()` color in RAM. `report()` logs a summary per scene (pixels drawn, pixels with a nonzero white
channel, SSD1306 style "on" pixels, gray4 histogram) and the RRGGBBWW rows of a region; `dump_get_pixel()`
logs what `Image::get_pixel()` returns; `level()` logs the gray4 level of a color; `time()` runs a draw 21 times and
logs min, median and max in microseconds.

- `mkimg.py` writes the test images into the current directory; `scene-common.yaml` (package, see its header)
  draws every image type and transparency, alpha images with explicit on and off colors, fills and anti-aliased
  text, then logs `DONE`. It builds for ESP8266, ESP32 and `host:` alike, so one capture per platform covers both
  the per-pixel path and the bulk path.
- `ana.py CAPTURE VARIANT PLATFORM` recomputes each scene on the host from the logged `get_pixel()` rows and the
  PNGs and prints `ALL_OK=True` when every pixel, histogram and summary matches. Its `expect()` rules encode what
  #23229 and #23226 changed; rewrite them for another change. Fills are checked by gray4 level only, text is printed.
- An `L` mode PNG with `type: GRAYSCALE` is read as alpha only; save test PNGs as RGB (mkimg.py does).
- The ESP8266 logger at 460800 with `tx_buffer_size: 1024` keeps up with the row dumps; at 115200 lines are lost.
- On a `host:` build the RGB565 scene shows channel values one count off the `get_pixel()` rows in about a quarter
  of the pixels (the bulk path rounds differently); PLATFORM `host` allows that, the other scenes match exactly.
