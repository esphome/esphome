import logging

import esphome.codegen as cg

# Re-exported from the shared definition; here it indexes the BOARDS
# metadata dicts, whose entries in boards.py spell the literal.
from esphome.const import KEY_FLASH_SIZE  # noqa: F401  # pylint: disable=unused-import
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority

_LOGGER = logging.getLogger(__name__)

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


KEY_PREFERENCES = "preferences"
# Words of RTC memory open to saved values; the crash handler keeps the rest
PREFERENCES_RTC_WORDS = PREFERENCES_RTC_NORMAL_WORDS + PREFERENCES_RTC_EBOOT_WORDS


def preference_words(value_bytes: int) -> int:
    """Words a saved value takes, checksum word included."""
    return (value_bytes + 3) // 4 + 1


def reserve_preferences(count: int, value_bytes: int, in_flash: bool = False) -> None:
    """Record saved values a component creates, so all of them are checked to fit; call from to_code."""
    if not CORE.is_esp8266:
        return
    reservations = CORE.data.setdefault(KEY_ESP8266, {}).setdefault(KEY_PREFERENCES, [])
    if not reservations:
        CORE.add_job(check_preferences_fit)
    reservations.append((count, preference_words(value_bytes), in_flash))


@coroutine_with_priority(CoroPriority.FINAL)
async def check_preferences_fit() -> None:
    """Fail the build when the reserved saved values cannot fit the space preferences.cpp has."""
    restore_from_flash = CORE.config[KEY_ESP8266].get(CONF_RESTORE_FROM_FLASH, False)
    flash_store = (
        PREFERENCES_FLASH_WORDS
        if restore_from_flash
        else PREFERENCES_FLASH_WORDS_WITHOUT_RESTORE
    )
    rtc_words = flash_words = 0
    for count, words, in_flash in CORE.data[KEY_ESP8266][KEY_PREFERENCES]:
        if in_flash or restore_from_flash:
            if words > flash_store:
                raise EsphomeError(
                    f"A saved value of {words} words does not fit the ESP8266 flash store of {flash_store} words"
                )
            flash_words += count * words
        else:
            # A value cannot span the two RTC regions, so the larger one bounds it
            if words > PREFERENCES_RTC_NORMAL_WORDS:
                raise EsphomeError(
                    f"A saved value of {words} words does not fit an ESP8266 RTC region of "
                    f"{PREFERENCES_RTC_NORMAL_WORDS} words"
                )
            rtc_words += count * words
    if flash_words > flash_store:
        raise EsphomeError(
            f"Saved values need {flash_words} words but the ESP8266 flash store has {flash_store}; "
            "use fewer or smaller saved values"
        )
    if rtc_words > PREFERENCES_RTC_WORDS:
        raise EsphomeError(
            f"Saved values need {rtc_words} words but ESP8266 RTC memory has {PREFERENCES_RTC_WORDS}; "
            "use fewer or smaller saved values, or set restore_from_flash: true"
        )
    if rtc_words > PREFERENCES_RTC_NORMAL_WORDS:
        _LOGGER.warning(
            "Saved values need %d of the %d words of ESP8266 RTC memory; values that straddle the two regions "
            "and saved settings that are not counted here may not all fit",
            rtc_words,
            PREFERENCES_RTC_WORDS,
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
