from esphome import pins
import esphome.codegen as cg
from esphome.components import audio_dac, socket
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.i2s_audio import (
    CONF_I2S_DOUT_PIN,
    CONF_STEREO,
    I2SAudioOut,
    i2s_audio_component_schema,
    register_i2s_audio_component,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PORT
from esphome.core import CORE

CODEOWNERS = ["@luar123"]

DEPENDENCIES = ["esp32", "i2s_audio"]
AUTO_LOAD = ["socket"]

CONF_HOSTNAME = "hostname"
CONF_AUDIO_DAC = "audio_dac"
CONF_MUTE_PIN = "mute_pin"

SNAPCLIENT_GIT_VERSION = "component"
SNAPCLIENT_GIT_REPO = "https://github.com/luar123/snapclient.git"

snapclient_ns = cg.esphome_ns.namespace("snapclient")
SnapClientComponent = snapclient_ns.class_(
    "SnapClientComponent", cg.Component, I2SAudioOut
)


def _consume_sockets(config):
    """Register socket needs for this component."""
    # upstream uses 10 sockets, but 7 are used for http server
    socket.consume_sockets(3, "snapclient")(config)
    return config


CONFIG_SCHEMA = cv.All(
    i2s_audio_component_schema(
        SnapClientComponent,
        default_sample_rate=44100,
        default_channel=CONF_STEREO,
        default_bits_per_sample="16bit",
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SnapClientComponent),
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_HOSTNAME, default=0): cv.domain,
            cv.Optional(CONF_PORT, default=1704): cv.port,
            cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_MUTE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_AUDIO_DAC): cv.use_id(audio_dac.AudioDac),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _consume_sockets,  # Register socket usage during validation
)


async def to_code(config):
    add_idf_component(name="espressif/esp-dsp", ref=">1.5.0")
    add_idf_component(name="espressif/mdns", ref=">1.9.0")
    for component in [
        "dsp_processor",
        "flac",
        "libbuffer",
        "libmedian",
        "lightsnapcast",
        "opus",
        "snapclient",
        "timefilter",
    ]:
        add_idf_component(
            name=component,
            ref=SNAPCLIENT_GIT_VERSION,
            repo=SNAPCLIENT_GIT_REPO,
            path=f"components/{component}",
        )
    if CONF_AUDIO_DAC not in config:
        add_idf_sdkconfig_option("CONFIG_USE_DSP_PROCESSOR", True)
        add_idf_sdkconfig_option("CONFIG_SNAPCLIENT_USE_SOFT_VOL", True)
    if CONF_NAME not in config:
        config[CONF_NAME] = CORE.name or ""
    add_idf_sdkconfig_option("CONFIG_SNAPSERVER_HOST", str(config[CONF_HOSTNAME]))
    add_idf_sdkconfig_option("CONFIG_SNAPSERVER_PORT", int(config[CONF_PORT]))
    if config[CONF_HOSTNAME] != 0:
        add_idf_sdkconfig_option("CONFIG_SNAPSERVER_USE_MDNS", False)
    add_idf_sdkconfig_option("CONFIG_SNAPCLIENT_NAME", config[CONF_NAME])
    add_idf_sdkconfig_option("CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES", 2)
    # add_idf_sdkconfig_option("CONFIG_LIBC_LOCKS_PLACE_IN_IRAM", True)
    # add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_CORE_LOCKING", False)

    # fix for esp-idf 5.4
    # cg.add_build_flag("-Wno-error=incompatible-pointer-types")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_i2s_audio_component(var, config)
    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
    cg.add(var.set_config(config[CONF_NAME], config[CONF_HOSTNAME], config[CONF_PORT]))
    if CONF_MUTE_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_MUTE_PIN])
        cg.add(var.set_mute_pin(pin))
    if audio_dac_config := config.get(CONF_AUDIO_DAC):
        aud_dac = await cg.get_variable(audio_dac_config)
        cg.add(var.set_audio_dac(aud_dac))
