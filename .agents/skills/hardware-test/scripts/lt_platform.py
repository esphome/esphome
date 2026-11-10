"""Default ltchiptool's -L to the newest installed LibreTiny platform.

ltchiptool reads its chip list from ~/.platformio/platforms/libretiny, which can be an older copy than the one a
build used (BK7238 UF2s then fail with "UF2 of unsupported family").
"""

import json
from pathlib import Path
import re
import sys


def _version(platform: Path) -> tuple[int, ...]:
    try:
        text = json.loads((platform / "platform.json").read_text(encoding="utf-8"))[
            "version"
        ]
    except (OSError, KeyError, ValueError):
        return ()
    # Numeric prefix of each part, so 1.9.0-dev still sorts
    return tuple(
        int(m[1]) for part in text.split(".")[:3] if (m := re.match(r"(\d+)", part))
    )


def with_platform(args: list[str]) -> list[str]:
    if any(a.startswith(("-L", "--libretiny-path")) for a in args):
        return args
    found = list((Path.home() / ".platformio" / "platforms").glob("libretiny*"))
    platforms = [p for p in found if _version(p)]
    for p in found:
        if p not in platforms:
            print(
                f"lt_platform: skipped {p} (no numeric version in platform.json)",
                file=sys.stderr,
            )
    if not platforms:
        print(
            f"lt_platform: no versioned LibreTiny platform in {[str(p) for p in found]}, using ltchiptool's default",
            file=sys.stderr,
        )
        return args
    chosen = max(platforms, key=_version)
    print(f"lt_platform: using {chosen}", file=sys.stderr)
    return ["-L", str(chosen), *args]
