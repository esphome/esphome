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
KEY_TIMER1_USERS = "timer1_users"
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
PREFERENCES_RTC_WORDS = PREFERENCES_RTC_NORMAL_WORDS + PREFERENCES_RTC_EBOOT_WORDS
PREFERENCES_FLASH_WORDS = 128
PREFERENCES_FLASH_WORDS_WITHOUT_RESTORE = 64
KEY_PREFERENCES = "preferences"


def preferences_flash_words(restore_from_flash: bool) -> int:
    """Size of the flash store for this configuration."""
    return (
        PREFERENCES_FLASH_WORDS
        if restore_from_flash
        else PREFERENCES_FLASH_WORDS_WITHOUT_RESTORE
    )


def preference_words(value_bytes: int) -> int:
    """Words a saved value takes, checksum word included (rtc_pref_bytes_to_words plus one)."""
    return (value_bytes + 3) // 4 + 1


def reserve_preferences(
    count: int, value_bytes: int, in_flash: bool | None = None
) -> None:
    """Record saved values a component creates; check_preferences_fit() adds them all up.

    in_flash is the value's own storage, as passed to make_preference(); None is the two-argument
    form, which follows restore_from_flash.
    """
    if in_flash is None:
        in_flash = CORE.config[KEY_ESP8266][CONF_RESTORE_FROM_FLASH]
    reservations = CORE.data.setdefault(KEY_ESP8266, {}).setdefault(KEY_PREFERENCES, [])
    reservations.extend([(preference_words(value_bytes), in_flash)] * count)


def _rtc_values_can_split(values: list[int]) -> bool:
    """Whether some order fills the two RTC regions without a value spanning them."""
    total = sum(values)
    if total <= PREFERENCES_RTC_NORMAL_WORDS:
        return True
    # Bit i of reachable is set when some subset of the values sums to i
    reachable = 1
    for words in values:
        reachable |= reachable << words
    lowest = max(0, total - PREFERENCES_RTC_EBOOT_WORDS)
    highest = min(total, PREFERENCES_RTC_NORMAL_WORDS)
    return any(reachable >> normal & 1 for normal in range(lowest, highest + 1))


@coroutine_with_priority(CoroPriority.FINAL)
async def check_preferences_fit() -> None:
    """Fail the build when the reserved saved values cannot fit the space preferences.cpp has."""
    flash_store = preferences_flash_words(
        CORE.config[KEY_ESP8266][CONF_RESTORE_FROM_FLASH]
    )
    reservations = CORE.data[KEY_ESP8266].get(KEY_PREFERENCES, [])
    flash_words = sum(w for w, in_flash in reservations if in_flash)
    rtc_values = [w for w, in_flash in reservations if not in_flash]
    if flash_words > flash_store:
        raise EsphomeError(
            f"Saved values need {flash_words} words but the ESP8266 flash store has {flash_store}; "
            "use fewer or smaller saved values"
        )
    if not _rtc_values_can_split(rtc_values):
        raise EsphomeError(
            f"Saved values need {sum(rtc_values)} words but ESP8266 RTC memory has {PREFERENCES_RTC_WORDS} in "
            f"regions of {PREFERENCES_RTC_NORMAL_WORDS} and {PREFERENCES_RTC_EBOOT_WORDS} that a value cannot "
            "span; use fewer or smaller saved values, or store them in flash"
        )
    if sum(rtc_values) > PREFERENCES_RTC_NORMAL_WORDS:
        _LOGGER.warning(
            "Saved values reserve %d of %d words of ESP8266 RTC memory; the fit depends on setup order, and "
            "saved settings that do not reserve are not counted",
            sum(rtc_values),
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
    use_timer1("the Arduino waveform generator (esp8266_pwm, ac_dimmer)")


def use_timer1(owner: str) -> None:
    """Record that a component drives hardware timer 1; check_timer1_users() rejects a second one.

    The ESP8266 has one general purpose timer with one interrupt. Two components that program it
    take it from each other, so the configuration is refused instead. Components that go through
    the Arduino waveform generator share it and register through require_waveform().
    """
    users = CORE.data.setdefault(KEY_ESP8266, {}).setdefault(KEY_TIMER1_USERS, set())
    users.add(owner)


@coroutine_with_priority(CoroPriority.FINAL)
async def check_timer1_users() -> None:
    """Fail the build when more than one component drives hardware timer 1."""
    users = sorted(CORE.data[KEY_ESP8266].get(KEY_TIMER1_USERS, ()))
    if len(users) > 1:
        raise EsphomeError(
            f"{' and '.join(users)} each need hardware timer 1 of the ESP8266, which only one of them can drive"
        )


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
