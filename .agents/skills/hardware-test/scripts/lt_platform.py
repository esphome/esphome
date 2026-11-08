"""Default ltchiptool's -L to the newest installed LibreTiny platform.

ltchiptool reads its chip list from ~/.platformio/platforms/libretiny, which can be an older copy than the one a
build used (BK7238 UF2s then fail with "UF2 of unsupported family").
"""

import json
from pathlib import Path


def _version(platform: Path) -> tuple[int, ...]:
    try:
        text = json.loads((platform / "platform.json").read_text(encoding="utf-8"))[
            "version"
        ]
        return tuple(int(p) for p in text.split(".")[:3])
    except (OSError, KeyError, ValueError):
        return ()


def with_platform(args: list[str]) -> list[str]:
    if "-L" in args or "--libretiny-path" in args:
        return args
    platforms = [
        p
        for p in (Path.home() / ".platformio" / "platforms").glob("libretiny*")
        if _version(p)
    ]
    if not platforms:
        return args
    return ["-L", str(max(platforms, key=_version)), *args]
