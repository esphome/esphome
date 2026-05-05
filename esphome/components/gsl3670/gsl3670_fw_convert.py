#!/usr/bin/env python3
"""
gsl3670_fw_convert.py
=====================
Converts a raw Silead GSL3670 firmware binary (.fw file, as distributed by
Seeed or the onitake/gsl-firmware repository) into the inline YAML format
accepted by the ESPHome gsl3670 touchscreen component.

Usage
-----
  python3 gsl3670_fw_convert.py <firmware.fw> [--output inline|c_array|yaml_file]

Output modes
------------
  inline     (default) – prints the 'firmware:' block to paste into your
             ESPHome YAML config directly under the touchscreen component.

  c_array    – prints a C++ const array suitable for hard-coding into a
               custom header (useful when you cannot use PROGMEM arrays via
               codegen, e.g. during early porting work).

  yaml_file  – writes a standalone firmware.yaml file you can !include.

Silead .fw binary format
-------------------------
Each record is 132 bytes:
  byte 0        : page index (written to register 0xB0)
  bytes 1-3     : padding / flags (ignored)
  bytes 4-131   : 128 bytes of firmware data for that page

This is the format produced by the silead_ts Windows driver extractor and is
the same format understood by the Linux kernel 'silead' driver.
"""

import argparse
import os
import sys

RECORD_SIZE = 132  # 4-byte header + 128-byte payload
DATA_SIZE = 128


def parse_firmware(path: str):
    """Return list of (page, data_bytes) tuples."""
    records = []
    with open(path, "rb") as f:
        raw = f.read()

    if len(raw) % RECORD_SIZE != 0:
        print(
            f"WARNING: file size {len(raw)} is not a multiple of {RECORD_SIZE}; "
            "trailing bytes will be ignored.",
            file=sys.stderr,
        )

    offset = 0
    while offset + RECORD_SIZE <= len(raw):
        page = raw[offset]
        data = raw[offset + 4 : offset + 4 + DATA_SIZE]
        records.append((page, bytes(data)))
        offset += RECORD_SIZE

    return records


def emit_inline_yaml(records, indent=4):
    pad = " " * indent
    lines = ["firmware:"]
    for page, data in records:
        hex_data = ", ".join(f"0x{b:02X}" for b in data)
        lines.append(f"{pad}- page: {page}")
        lines.append(f"{pad}  data: [{hex_data}]")
    return "\n".join(lines)


def emit_c_array(records, array_name="gsl3670_firmware"):
    lines = [
        '#include "gsl3670_touchscreen.h"',
        "",
        f"// Auto-generated from firmware binary – {len(records)} pages",
        "// Place this file in your ESPHome 'includes:' directory.",
        "",
        f"static const esphome::gsl3670::GSL3670FirmwareRecord {array_name}[] PROGMEM = {{",
    ]
    for i, (page, data) in enumerate(records):
        hex_bytes = ", ".join(f"0x{b:02X}" for b in data)
        comma = "" if i == len(records) - 1 else ","
        lines.append(f"  {{ 0x{page:02X}, {{ {hex_bytes} }} }}{comma}")
    lines += [
        "};",
        "",
        f"static const size_t {array_name}_len = {len(records)};",
    ]
    return "\n".join(lines)


def emit_yaml_file(records):
    """Produces a file that can be !include'd under the touchscreen component."""
    return emit_inline_yaml(records, indent=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", help="Path to the .fw binary file")
    parser.add_argument(
        "--output",
        choices=["inline", "c_array", "yaml_file"],
        default="inline",
        help="Output format (default: inline)",
    )
    parser.add_argument(
        "--out-file",
        metavar="FILE",
        default=None,
        help="Write output to FILE instead of stdout",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.firmware):
        sys.exit(f"ERROR: {args.firmware!r} not found")

    records = parse_firmware(args.firmware)
    if not records:
        sys.exit("ERROR: no valid records found in firmware file")

    print(f"Parsed {len(records)} firmware pages", file=sys.stderr)

    if args.output == "inline":
        out = emit_inline_yaml(records)
    elif args.output == "c_array":
        out = emit_c_array(records)
    else:
        out = emit_yaml_file(records)

    if args.out_file:
        with open(args.out_file, "w") as f:
            f.write(out)
        print(f"Written to {args.out_file}", file=sys.stderr)
    else:
        print(out)


if __name__ == "__main__":
    main()
