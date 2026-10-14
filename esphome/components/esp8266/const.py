import esphome.codegen as cg

# Re-exported from the shared definition; here it indexes the BOARDS
# metadata dicts, whose entries in boards.py spell the literal.
from esphome.const import KEY_FLASH_SIZE  # noqa: F401  # pylint: disable=unused-import
from esphome.core import CORE

KEY_ESP8266 = "esp8266"
KEY_BOARD = "board"
KEY_PIN_INITIAL_STATES = "pin_initial_states"
CONF_RESTORE_FROM_FLASH = "restore_from_flash"
CONF_EARLY_PIN_INIT = "early_pin_init"
CONF_ENABLE_SERIAL = "enable_serial"
CONF_ENABLE_SERIAL1 = "enable_serial1"
KEY_WAVEFORM_REQUIRED = "waveform_required"
KEY_SERIAL_REQUIRED = "serial_required"
KEY_SERIAL1_REQUIRED = "serial1_required"
# Set for the native (non-PlatformIO) toolchain's build generator
KEY_SCANF_FLOAT = "scanf_float"
# Force-included std::__throw_* overrides, relative to the src dir
THROW_STUBS_HEADER = "esphome/components/esp8266/throw_stubs.h"
# Per-board flash-layout override consumed by board_ld_script()
KEY_LDSCRIPT = "ldscript"
# Crash-decode tools, resolved once per run by __init__
KEY_DECODE_TOOLS = "decode_tools"

# Saved-settings space in 4-byte words; preferences.cpp gets these as defines. A value cannot span the two
# RTC regions, and the flash store is smaller when only explicit flash preferences use it.
PREFERENCES_RTC_NORMAL_WORDS = 78
PREFERENCES_RTC_EBOOT_WORDS = 32
PREFERENCES_FLASH_WORDS = 128
PREFERENCES_FLASH_WORDS_WITHOUT_RESTORE = 64


def preference_words(value_bytes: int) -> int:
    """Words a saved value takes, checksum word included."""
    return (value_bytes + 3) // 4 + 1


def preference_slots(value_bytes: int, in_flash: bool, restore_from_flash: bool) -> int:
    """How many values of this size fit where make_preference() puts them."""
    words = preference_words(value_bytes)
    if in_flash or restore_from_flash:
        store = (
            PREFERENCES_FLASH_WORDS
            if restore_from_flash
            else PREFERENCES_FLASH_WORDS_WITHOUT_RESTORE
        )
        return store // words
    return sum(
        region // words
        for region in (PREFERENCES_RTC_NORMAL_WORDS, PREFERENCES_RTC_EBOOT_WORDS)
    )


# esp8266 namespace is already defined by arduino, manually prefix esphome
esp8266_ns = cg.global_ns.namespace("esphome").namespace("esp8266")


def require_waveform() -> None:
    """Mark that Arduino waveform/PWM support is required.

    Call this from components that need the Arduino waveform generator
    (startWaveform, stopWaveform, analogWrite, Tone, Servo).

    If no component calls this, the waveform code is excluded from the build
    to save ~596 bytes of RAM and 464 bytes of flash.

    Example:
        from esphome.components.esp8266.const import require_waveform

        async def to_code(config):
            require_waveform()
    """
    CORE.data.setdefault(KEY_ESP8266, {})[KEY_WAVEFORM_REQUIRED] = True


def enable_serial() -> None:
    """Mark that Arduino Serial (UART0) is required.

    Call this from components that use the global Serial object.
    If no component calls this, Serial is excluded from the build
    to save 32 bytes of RAM.

    Example:
        from esphome.components.esp8266.const import enable_serial

        async def to_code(config):
            enable_serial()
    """
    CORE.data.setdefault(KEY_ESP8266, {})[KEY_SERIAL_REQUIRED] = True


def enable_serial1() -> None:
    """Mark that Arduino Serial1 (UART1) is required.

    Call this from components that use the global Serial1 object.
    If no component calls this, Serial1 is excluded from the build
    to save 32 bytes of RAM.

    Example:
        from esphome.components.esp8266.const import enable_serial1

        async def to_code(config):
            enable_serial1()
    """
    CORE.data.setdefault(KEY_ESP8266, {})[KEY_SERIAL1_REQUIRED] = True


BUILD_FLASH_MODES = ("qio", "qout", "dio", "dout")
