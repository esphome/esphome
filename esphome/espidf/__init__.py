"""ESP-IDF direct build support.

Deliberately light: the upload fast path imports submodules of this
package without the esp32 component package, so nothing here may pull
in codegen or validation.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def variant_to_idf_target(variant: str) -> str:
    """Map an esp32 variant name (e.g. "ESP32S3") to its ESP-IDF target name."""
    return variant.lower().replace("-", "")


def parse_sdkconfig(path: Path) -> dict[str, str]:
    """Map each CONFIG_X=value line of a text sdkconfig to its raw value.

    Comment lines ("# CONFIG_X is not set") are skipped and quoted values
    are unquoted.
    """
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("CONFIG_"):
            continue
        name, _, value = line.partition("=")
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] == '"':
            value = value[1:-1]
        result[name] = value
    return result
