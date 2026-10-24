---
name: display-test
description: Test an ESPHome display driver on a real panel with a camera. Use when a task asks to verify a display (LCD, e-paper, OLED, RLCD) on hardware, check orientation/rotation, partial updates, colours, contrast, or that text and shapes render correctly. Covers finding the USB port, flashing a test pattern, capturing the panel with any camera, and reading the photo.
allowed-tools: Read, Write, Edit, Bash, Glob, Grep
---

# Display testing with a camera

Point a camera at the panel to get visual evidence that a driver renders correctly. Any camera
and capture tool works; this skill covers the common ones. `$SKILL` below is this directory
(`.agents/skills/display-test`).

## Ask before using a camera

A camera can show people and rooms, not only the panel. Before the first capture, ask the user
for permission and for which camera to use. List the cameras the host has (the commands below
show how), name the one you intend to use, and wait for the user to confirm it. Do not pick a
camera yourself, and do not fall back to another camera if the chosen one fails; ask again.

The permission covers this session and the camera the user named. Ask again for a different
camera or in a new session. If the user declines, stop and report that the display was not
checked visually.

## Capturing a frame

Grab a still image, skipping the first few frames so auto-exposure and white balance settle.
Read the JPEG with the Read tool; the image is shown visually.

### macOS

The bundled Swift tool uses AVFoundation and avoids the frame-grabbing problems some USB
cameras have with other tools:

```bash
mkdir -p /tmp/camsnap /tmp/display-test
swiftc -O "$SKILL/scripts/camsnap.swift" -o /tmp/camsnap/camsnap
/tmp/camsnap/camsnap <camera-name-substring> /tmp/display-test/after.jpg 30
```

The camera argument is a case-insensitive substring of the name AVFoundation reports. If it does
not match, the tool lists the cameras it found, so a run with a name that matches nothing lists
the cameras without capturing. Use a substring that matches only the camera the user approved.
It creates the output directory if needed and reports encoding or write errors on stderr. Other
options include `ffmpeg` with `avfoundation` input, `imagesnap`, or Photo Booth's still capture.

### Linux

Use `ffmpeg` with the v4l2 input, `fswebcam`, or `v4l2-ctl`. With `ffmpeg`, `-ss 2` after the
input discards the first two seconds of frames while exposure settles:

```bash
mkdir -p /tmp/display-test
ffmpeg -f v4l2 -i /dev/video0 -ss 2 -frames:v 1 -y /tmp/display-test/after.jpg
# or
fswebcam -d /dev/video0 --skip 10 /tmp/display-test/after.jpg
```

List devices with `v4l2-ctl --list-devices`.

### Windows

Use `ffmpeg` with the DirectShow input (`-ss 2` skips the first frames, as on Linux), or any
camera application that can save a still image:

```powershell
mkdir C:\temp -Force | Out-Null
ffmpeg -f dshow -list_devices true -i dummy   # find the device name
ffmpeg -f dshow -i video="<device name>" -ss 2 -frames:v 1 -y C:\temp\after.jpg
```

### Camera caveats

- The board may sit at an angle. Do not trust absolute pixel positions or aspect ratios from the
  photo; judge orientation, ordering and approximate spacing, using the corner markers as
  references.
- Reflections or a glossy cover can brighten one side. Capture again if a region is unclear.
- A moving pattern is caught at an arbitrary phase. Take several captures, or use a static marker
  per position.

## Board and port

Find the port; names change on every replug:

```bash
python3 "$SKILL/../hardware-test/scripts/ports.py"
```

Native-USB ESP32-S3/C3/C6/H2 boards show up as `USB JTAG/serial debug unit` and need
`logger: hardware_uart: USB_SERIAL_JTAG`.

## Test pattern

Write a scratch config outside the checkout. Use one `name:` per board and include:

- a white background, a black border and a filled square at the centre, drawn in the first
  update (full-window check and scale);
- a 60x20 bar near each corner, one bar per later update with `auto_clear_enabled: false`, so
  each bar is written by its own partial window. The bars are wider than tall, which shows a
  90 or 270 degree rotation; the corners exercise the window alignment;
- a 20x20 square under the left end of the top-left bar, drawn in the same update, which marks
  the top-left corner and shows a 180 degree rotation or a mirrored image.

Use `Color::BLACK` and `Color::WHITE` in lambdas. Do not use the old `COLOR_OFF`/`COLOR_ON`
constants; black must mean black.

A ready template is in `$SKILL/templates/display-pattern.yaml`. Substitute the display platform,
pins and (for a driver with fonts) a font. The marker pattern needs no font.

## Build, flash, capture

Run from the worktree under test so `python -m esphome` imports the checkout, not an installed
copy:

```bash
cd <worktree>
python3 -c "import esphome; print(esphome.__file__)"   # must print the worktree path
PYTHONUNBUFFERED=1 python3 -m esphome compile /tmp/display-test/display-pattern.yaml
PYTHONUNBUFFERED=1 python3 -m esphome upload /tmp/display-test/display-pattern.yaml \
  --device /dev/cu.usbmodemXXXX
sleep 6   # boot plus a few update intervals
<capture command> /tmp/display-test/after.jpg
```

The device path differs by OS and host; use the path printed by `ports.py`. If the USB console
wedges during flashing (esptool `No serial data received`), replug the board and retry. A board
powered only by a battery may need the USB cable for flashing; the panel keeps its image when
power is removed, which is useful for checking that an image is bistable.

## What to check

1. **Orientation**: the extra square sits under the bar in the expected top-left corner. If it is
   in another corner, the image is rotated or mirrored. Test `rotation:` values with the same
   pattern. The logical width and height swap at 90 and
   270 degrees, so the 60x20 corner bars should then appear taller than wide on the physical
   panel.
2. **Coverage**: the border touches all four edges, with no dead rows or columns at the edges.
3. **Partial updates**: with `auto_clear_enabled: false`, draw one marker per update. Each should
   appear exactly at its requested position with no smearing, offset or mirrored placement.
   Include all four corners.
4. **Text and contrast**: sharp glyphs, correct polarity (black text on white, not inverted), no
   ghosting from the previous frame.
5. **Speed**: if the driver logs transfer sizes, check that a full refresh and a partial window
   take the expected times.

Record the build commit, board, config, and the captured image path when reporting results.

## When a partial update looks wrong

Take a marker-only capture before assuming the mapping is wrong: the camera's perspective makes
absolute positions deceptive. If markers land in mirrored positions, the controller's address
order (`MADCTL` MX/MY/MV) or the RAM block mapping is inverted for the window; verify the
full-frame path separately, then fix the window computation.

## Add findings

Add new capture tools, panel quirks, or recovery steps here so the next session starts ahead.
