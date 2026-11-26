# lvgl

## Screen snapshot over the serial log (#23227)

Visual proof without a camera: `scripts/lvgl_snapshot/snapshot.yaml` (package) adds `-DLV_USE_SNAPSHOT=1`
(ESPHome's `lv_conf.h` leaves LV_ defines it does not manage to build flags, so this works on ESP-IDF), includes
`snap.hpp` and adds a `snapshot` button. `lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565)` takes about
25 ms for 320x240 on an S3; the image is logged as run length hex, 4 lines per 20 ms interval, between `SNAPBEGIN`
and `SNAPEND` (line count, runs, checksum). `unsnap.py LOG OUTPREFIX` turns each one back into a PNG and checks the
checksum.

- Capture the serial port (USB Serial/JTAG on the S3-BOX-3), not the API log stream: the API drops about half of
  the lines of a dump.
- `snaptest::dump_series(name, chart, series, with_x)` logs a chart series in logical order (oldest first, `-` for
  an empty slot) so each chart action can be checked against the data as well as the picture.
- When `lvgl.chart.update` makes `point_count` smaller, LVGL keeps the oldest slots counted from the start point, so
  newer values can disappear; that is LVGL behaviour, not a component bug.
