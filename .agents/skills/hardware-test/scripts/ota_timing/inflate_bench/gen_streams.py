"""Write bench_streams.hpp: one 128 KB slice per image, raw deflate at level 9 with 4 KB and 32 KB windows.
Usage: gen_streams.py NAME=IMAGE[@OFFSET]... [--windows 12,15]
Each image gives one case per window. Keep the total near 130 KB on a BK7231N: an image with about 330 KB of extra
incompressible data failed its OTA ("Writing OTA data to flash memory failed"); use one image and --windows 12 there."""

import argparse
from pathlib import Path
import zlib

SLICE = 128 * 1024
ap = argparse.ArgumentParser()
ap.add_argument("images", nargs="+")
ap.add_argument("--windows", default="12,15")
args = ap.parse_args()
arrays, cases, total = [], [], 0
for spec in args.images:
    name, _, rest = spec.partition("=")
    path, _, offset = rest.partition("@")
    raw = Path(path).read_bytes()[int(offset or 0) :][:SLICE]
    for wbits in (int(w) for w in args.windows.split(",")):
        data = zlib.compress(raw, 9, -wbits)
        total += len(data)
        var = f"{name}_w{wbits}"
        arrays.append(
            f"static const uint8_t {var}[{len(data)}] = {{{','.join(map(str, data))}}};"
        )
        cases.append(f'{{"{name}", {var}, sizeof({var}), {len(raw)}u, {1 << wbits}u}}')
        print(f"{var}: {len(raw)} -> {len(data)} bytes")
head = "#pragma once\n#include <cstddef>\n#include <cstdint>\n"
case_struct = (
    "struct BenchCase {\n  const char *name;\n  const uint8_t *data;\n  size_t len;\n  size_t raw;\n"
    "  unsigned enc_window;\n};\n"
)
body = (
    "\n".join(arrays)
    + "\n"
    + case_struct
    + "static const BenchCase BENCH_CASES[] = {"
    + ", ".join(cases)
    + "};\n"
)
Path("bench_streams.hpp").write_text(head + body, encoding="utf-8")
print(f"total {total} bytes of streams")
