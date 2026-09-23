"""Generate boards.py from arduino-pico board definitions.

Usage: python esphome/components/rp2/generate_boards.py <arduino-pico-path>
"""

import json
from pathlib import Path
import re
import subprocess
import sys

from jinja2 import Environment, FileSystemLoader

# Map arduino-pico pin defines to ESPHome-friendly names
PIN_NAME_MAP = {
    "LED": "LED",
    "WIRE0_SDA": "SDA",
    "WIRE0_SCL": "SCL",
    "WIRE1_SDA": "SDA1",
    "WIRE1_SCL": "SCL1",
    "SPI0_MISO": "MISO",
    "SPI0_MOSI": "MOSI",
    "SPI0_SCK": "SCK",
    "SPI0_SS": "SS",
    "SERIAL1_TX": "TX",
    "SERIAL1_RX": "RX",
}

# arduino-pico maps pins >= 64 to CYW43 wireless chip GPIOs (pin - 64)
CYW43_GPIO_OFFSET = 64
# CYW43 has 3 GPIOs: 0=LED, 1=VBUS_SENSE, 2=REG_ON
CYW43_GPIO_COUNT = 3

# Max GPIO pin per MCU (hardware specs from datasheets)
MCU_MAX_PIN = {
    "rp2040": 29,  # GPIO 0-29
    "rp2350": 47,  # GPIO 0-47 (RP2350B; A-die boards are narrowed to 29 below)
}
DEFAULT_MAX_PIN = 29
# The RP2350 currently comes in two die variants: RP2350A exposes GPIO 0-29,
# RP2350B GPIO 0-47. Variant headers declare the die via PICO_RP2350A
# (1 = A, 0 = B).
RP2350_DIE_A = "A"
RP2350_DIE_B = "B"
RP2350A_MAX_PIN = 29
# Key recording the die letter on RP2350 board entries. Holds a letter rather
# than a bool so a future die can be named instead of forced into "not A".
RP2350_DIE_KEY = "die"

PIN_DEFINE_RE = re.compile(r"#define\s+PIN_(\w+)\s+\((\d+)u\)")
# Accepts the literal forms seen in these headers: 1, (1), 1u, (1u)
RP2350A_DEFINE_RE = re.compile(r"#define\s+PICO_RP2350A\s+(\S+)")
# Only PICO_RP2350A exists today. A define for any other die letter means the
# A/B assumption below no longer holds. The trailing \b keeps this from
# matching unrelated names such as PICO_RP2350_A2_SUPPORTED.
OTHER_DIE_DEFINE_RE = re.compile(r"#define\s+PICO_RP2350(?!A\b)([B-Z])\b")
RP2350A_MENU_PLACEHOLDER = "__PICO_RP2350A"


def parse_variant_pins(variant_dir: Path) -> dict[str, int]:
    """Parse pins_arduino.h and return mapped pin names."""
    header = variant_dir / "pins_arduino.h"
    if not header.exists():
        return {}

    pins = {}
    for match in PIN_DEFINE_RE.finditer(header.read_text(encoding="utf-8")):
        raw_name = match.group(1)
        value = int(match.group(2))
        if raw_name in PIN_NAME_MAP:
            pins[PIN_NAME_MAP[raw_name]] = value
    return pins


def parse_variant_rp2350_die(variant_dir: Path) -> str | None:
    """Return the RP2350 die letter the variant declares, or None if unknown.

    Generic boards leave the die a build-time menu choice (PICO_RP2350A is set
    to a __PICO_RP2350A placeholder rather than a literal); those return None,
    meaning the die is genuinely unknown at code generation time. They keep the
    permissive B-die pin range, but that is a fallback and must not be recorded
    as a known die.

    A missing or unrecognized define raises: silently treating it as B-die
    would widen pin validation back to GPIO 47 on A-die boards, so a framework
    bump that changes the header format must fail loudly here instead. The same
    goes for a die beyond A and B: PICO_RP2350A is a yes/no answer about the A
    die, so "not A" can only be read as B while A and B are the whole family.
    """
    header = variant_dir / "pins_arduino.h"
    text = header.read_text(encoding="utf-8") if header.exists() else ""
    if other_die := OTHER_DIE_DEFINE_RE.search(text):
        raise ValueError(
            f"{header}: found a PICO_RP2350{other_die.group(1)} define; the "
            "RP2350 gained a die beyond A and B, so PICO_RP2350A being 0 no "
            "longer means the B die"
        )
    match = RP2350A_DEFINE_RE.search(text)
    if match is None:
        raise ValueError(
            f"{header}: no PICO_RP2350A define found; cannot classify the "
            "RP2350 die (A exposes GPIO 0-29, B exposes GPIO 0-47)"
        )
    value = match.group(1)
    if value == RP2350A_MENU_PLACEHOLDER:
        return None
    literal = value.strip("()u")
    if not literal.isdigit():
        raise ValueError(
            f"{header}: unrecognized PICO_RP2350A value {value!r}; cannot "
            "classify the RP2350 die (A exposes GPIO 0-29, B exposes GPIO 0-47)"
        )
    return RP2350_DIE_A if int(literal) == 1 else RP2350_DIE_B


def load_boards(arduino_pico_path: Path) -> tuple[dict, dict]:
    """Load all board definitions and return (board_pins, boards) dicts."""
    json_dir = arduino_pico_path / "tools" / "json"
    variants_dir = arduino_pico_path / "variants"

    board_pins = {}
    boards = {}
    variant_pins_cache: dict[str, dict[str, int]] = {}
    variant_die_cache: dict[str, str | None] = {}

    for json_file in sorted(json_dir.glob("*.json")):
        board_name = json_file.stem
        with json_file.open(encoding="utf-8") as f:
            data = json.load(f)

        build = data.get("build", {})
        mcu = build.get("mcu", "rp2040")
        variant = build.get("variant", board_name)
        name = data.get("name", board_name)
        vendor = data.get("vendor", "")

        display_name = f"{vendor} {name}".strip() if vendor else name

        extra_flags = build.get("extra_flags", "")
        has_wifi = "PICO_CYW43_SUPPORTED=1" in extra_flags

        max_pin = MCU_MAX_PIN.get(mcu, DEFAULT_MAX_PIN)
        die: str | None = None
        if mcu == "rp2350":
            if variant not in variant_die_cache:
                variant_die_cache[variant] = parse_variant_rp2350_die(
                    variants_dir / variant
                )
            die = variant_die_cache[variant]
            if die == RP2350_DIE_A:
                max_pin = RP2350A_MAX_PIN

        board_entry: dict = {
            "name": display_name,
            "mcu": mcu,
            "max_pin": max_pin,
        }
        if mcu == "rp2350":
            # Recorded explicitly because max_pin cannot express the die:
            # 29 also means RP2040, and 47 also means "die not known yet".
            board_entry[RP2350_DIE_KEY] = die
        if has_wifi:
            board_entry["wifi"] = True
        boards[board_name] = board_entry

        # Get pins for this variant
        if variant not in variant_pins_cache:
            variant_dir = variants_dir / variant
            variant_pins_cache[variant] = parse_variant_pins(variant_dir)

        pins = variant_pins_cache[variant]
        if pins:
            max_pin = boards[board_name]["max_pin"]
            cyw43_max = CYW43_GPIO_OFFSET + CYW43_GPIO_COUNT - 1
            # Filter out placeholder values (e.g. 99 = "not connected")
            filtered = {
                name: value
                for name, value in pins.items()
                if value <= max_pin or CYW43_GPIO_OFFSET <= value <= cyw43_max
            }
            if filtered:
                board_pins[board_name] = filtered

    # Compute max_virtual_pin per board from pin maps
    for board_name, pins in board_pins.items():
        if isinstance(pins, str):
            continue
        virtual_pins = [v for v in pins.values() if v >= CYW43_GPIO_OFFSET]
        if virtual_pins and board_name in boards:
            boards[board_name]["max_virtual_pin"] = max(virtual_pins)

    # Deduplicate: if board pins match its variant's pins, use string alias
    for board_name in list(board_pins.keys()):
        if board_name not in boards:
            continue
        build_variant = _get_variant(json_dir / f"{board_name}.json")
        if (
            build_variant
            and build_variant != board_name
            and build_variant in board_pins
            and board_pins[board_name] == board_pins[build_variant]
        ):
            board_pins[board_name] = build_variant

    return board_pins, boards


def _get_variant(json_file: Path) -> str | None:
    """Get variant name from a board JSON file."""
    if not json_file.exists():
        return None
    with json_file.open(encoding="utf-8") as f:
        data = json.load(f)
    return data.get("build", {}).get("variant")


_TEMPLATE_DIR = Path(__file__).parent


def _format_pins(pins: dict[str, int] | str) -> str:
    """Jinja2 filter to format a pin dict or alias as Python source."""
    if isinstance(pins, str):
        return repr(pins)
    items = ", ".join(f"{k!r}: {v}" for k, v in sorted(pins.items()))
    return f"{{{items}}}"


_jinja_env = Environment(
    loader=FileSystemLoader(_TEMPLATE_DIR), keep_trailing_newline=True
)
_jinja_env.filters["format_pins"] = _format_pins
_jinja_env.filters["repr"] = repr


def generate(arduino_pico_path: Path) -> str:
    """Generate boards.py content."""
    board_pins, boards = load_boards(arduino_pico_path)

    template = _jinja_env.get_template("boards.jinja2")
    content = template.render(
        cyw43_gpio_offset=CYW43_GPIO_OFFSET,
        cyw43_max_gpio=CYW43_GPIO_OFFSET + CYW43_GPIO_COUNT - 1,
        default_max_pin=DEFAULT_MAX_PIN,
        rp2350_die_key=RP2350_DIE_KEY,
        board_pins=sorted(board_pins.items()),
        boards=sorted(boards.items()),
    )

    # Format output to match pre-commit ruff formatting
    result = subprocess.run(
        [sys.executable, "-m", "ruff", "format", "--stdin-filename", "boards.py"],
        input=content.encode(),
        capture_output=True,
        check=True,
    )
    return result.stdout.decode()


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <arduino-pico-path>", file=sys.stderr)
        sys.exit(1)

    arduino_pico_path = Path(sys.argv[1])
    if not (arduino_pico_path / "tools" / "json").exists():
        print(f"Error: {arduino_pico_path}/tools/json not found", file=sys.stderr)
        sys.exit(1)

    output = generate(arduino_pico_path)
    output_file = Path(__file__).parent / "boards.py"
    output_file.write_text(output, encoding="utf-8")
    print(f"Generated {output_file}")


if __name__ == "__main__":
    main()
