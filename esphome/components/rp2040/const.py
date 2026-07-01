import esphome.codegen as cg

KEY_BOARD = "board"
KEY_LWIP_OPTS = "lwip_opts"
KEY_RP2040 = "rp2040"
KEY_PIO_FILES = "pio_files"
KEY_VARIANT = "variant"

VARIANT_RP2040 = "RP2040"
VARIANT_RP2350 = "RP2350"
VARIANTS = [
    VARIANT_RP2040,
    VARIANT_RP2350,
]

VARIANT_FRIENDLY = {
    VARIANT_RP2040: "RP2040",
    VARIANT_RP2350: "RP2350",
}

# Map BOARDS[board]["mcu"] (lowercase) to canonical variant constant
MCU_TO_VARIANT = {
    "rp2040": VARIANT_RP2040,
    "rp2350": VARIANT_RP2350,
}

# Default board chosen when only `variant` is specified — the Raspberry Pi
# Foundation reference boards (Pico W / Pico 2 W).
STANDARD_BOARDS = {
    VARIANT_RP2040: "rpipicow",
    VARIANT_RP2350: "rpipico2w",
}

rp2040_ns = cg.esphome_ns.namespace("rp2040")
