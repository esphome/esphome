"""Write the trimmed LibreTiny UF2 the CLI would send (esphome#21642) next to firmware.uf2 and print its path.
Usage: trim_uf2.py FIRMWARE_UF2   (run with the phase's checkout on PYTHONPATH)
Prints the input path unchanged when the tree has no device_ota_uf2() or there is nothing to drop."""

from pathlib import Path
import sys

src = Path(sys.argv[1])
try:
    from esphome.components.libretiny.uf2 import device_ota_uf2
except ImportError:
    print(src)
    sys.exit(0)
data = src.read_bytes()
if (trimmed := device_ota_uf2(data)) is None:
    print(src)
    sys.exit(0)
out = src.with_name("firmware.ota.uf2")
out.write_bytes(trimmed)
print(f"trimmed {len(data)} -> {len(trimmed)} bytes", file=sys.stderr)
print(out)
