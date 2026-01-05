import esphome.codegen as cg
from esphome.core import CORE

KEY_ESP8266 = "esp8266"
KEY_BOARD = "board"
KEY_PIN_INITIAL_STATES = "pin_initial_states"
CONF_RESTORE_FROM_FLASH = "restore_from_flash"
CONF_EARLY_PIN_INIT = "early_pin_init"
KEY_FLASH_SIZE = "flash_size"
KEY_WAVEFORM_REQUIRED = "waveform_required"

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
