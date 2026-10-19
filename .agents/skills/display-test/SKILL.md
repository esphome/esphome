---
name: display-test
description: Test an ESPHome display driver on a real panel with the bench camera. Use when a task asks to verify a display (LCD, e-paper, OLED, RLCD) on hardware, check orientation/rotation, partial updates, colours, contrast, or that text and shapes render correctly. Covers finding the USB port, flashing a test pattern, capturing the panel with the UC60 camera, and reading the photo.
allowed-tools: Read, Write, Edit, Bash, Glob, Grep
---

# Display testing with the bench camera

The bench camera is the **UC60 Video** AVFoundation device. It looks down at the display board on the
bench. `ffmpeg` capture from it is unreliable; use the small Swift tool bundled here.

`$SKILL` below is this directory (`.agents/skills/display-test`).

## Camera

Build once per machine (the binary is not checked in):

```bash
mkdir -p /tmp/camsnap
swiftc -O "$SKILL/scripts/camsnap.swift" -o /tmp/camsnap/camsnap
```

Capture one frame after the exposure settles:

```bash
/tmp/camsnap/camsnap UC60 /tmp/display-test/after.jpg 30
```

The third argument is the number of frames to skip (default 30). If the name does not match, the tool
prints the cameras it found. Read the JPEG with the Read tool; the image is shown visually.

Camera caveats:

- The board sits at an angle. Do not trust absolute pixel positions or aspect ratios from the photo.
  Judge orientation, ordering and approximate spacing, and use corner markers as references.
- Reflections and the acrylic cover can brighten one side. Capture again if a region is unclear.
- A single frame catches a moving pattern at an arbitrary phase. For a moving test, take several
  captures or use a static marker per position.

## Board and port

Find the port; names change on every replug:

```bash
python3 "$SKILL/../hardware-test/scripts/ports.py"
```

Native-USB ESP32-S3/C3/C6/H2 boards show up as `USB JTAG/serial debug unit` and need
`logger: hardware_uart: USB_SERIAL_JTAG`.

## Test pattern

Write a scratch config outside the checkout. Use one `name:` per board and include:

- a white background and a black border;
- a filled black square in each of the four corners (orientation and full-window check);
- a centre marker (orientation and scale);
- for partial-update drivers, draw one additional marker per update with
  `auto_clear_enabled: false`, so each marker is written by its own window. Corners and edges exercise
  the window alignment.

Use `Color::BLACK` and `Color::WHITE` in lambdas. Do not use the old `COLOR_OFF`/`COLOR_ON`
constants; black must mean black.

A ready template is in `$SKILL/templates/display-pattern.yaml`. Substitute the display platform, pins
and (for a driver with fonts) a font. The marker pattern needs no font.

## Build, flash, capture

Run from the worktree under test so `python -m esphome` imports the checkout, not an installed copy:

```bash
cd <worktree>
python3 -c "import esphome; print(esphome.__file__)"   # must print the worktree path
PYTHONUNBUFFERED=1 python3 -m esphome compile /tmp/display-test/display-pattern.yaml
PYTHONUNBUFFERED=1 python3 -m esphome upload /tmp/display-test/display-pattern.yaml \
  --device /dev/cu.usbmodemXXXX
sleep 6   # boot plus a few update intervals
/tmp/camsnap/camsnap UC60 /tmp/display-test/after.jpg 30
```

If the USB console wedges during flashing (esptool `No serial data received`), replug the board and
retry. A board powered only by the 18650 may need the USB cable for flashing; the panel keeps its
image when power is removed, which is useful for checking that an image is bistable.

## What to check

1. **Orientation**: the corner markers are in the expected corners and the image is not mirrored or
   upside down. Test `rotation:` values with the same pattern; the logical width/height swap, so a
   marker that is 20x60 in the lambda should appear 60x20 on the physical landscape panel.
2. **Coverage**: the border touches all four edges, with no dead rows or columns at the edges.
3. **Partial updates**: with `auto_clear_enabled: false`, draw one marker per update. Each should
   appear exactly at its requested position with no smearing, offset or mirrored placement. Include
   all four corners and both edges.
4. **Text and contrast**: sharp glyphs, correct polarity (black text on white, not inverted), no
   ghosting from the previous frame.
5. **Speed**: if the driver logs transfer sizes, check that a full refresh and a partial window take
   the expected times.

Record the build commit, board, config, and the captured image path when reporting results.

## When a partial update looks wrong

Take a marker-only capture before assuming the mapping is wrong: the camera's perspective makes
absolute positions deceptive. If markers land in mirrored positions, the controller's address order
(`MADCTL` MX/MY/MV) or the RAM block mapping is inverted for the window; verify the full-frame path
separately, then fix the window computation.

## Add findings

Add new camera names, panel quirks, or recovery steps here so the next session starts ahead.
