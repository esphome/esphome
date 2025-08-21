from esphome import pins
import esphome.codegen as cg
from esphome.components import audio_dac
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

CONF_HOSTNAME = "hostname"
CONF_AUDIO_DAC = "audio_dac"
CONF_MUTE_PIN = "mute_pin"
CONF_WEBSERVER_PORT = "webserver_port"

snapclient_ns = cg.esphome_ns.namespace("snapclient")
SnapClientComponent = snapclient_ns.class_(
    "SnapClientComponent", cg.Component, I2SAudioOut
)

CONFIG_SCHEMA = cv.All(
    i2s_audio_component_schema(
        SnapClientComponent,
        default_sample_rate=16000,
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
            cv.Optional(CONF_WEBSERVER_PORT): cv.port,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(esp_idf=cv.Version(5, 1, 1)),
)


async def to_code(config):
    add_idf_component(name="espressif/esp-dsp", ref=">1.5.0")
    add_idf_component(name="espressif/mdns", ref=">1.2.3")
    add_idf_component(
        name="lightsnapcast",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/lightsnapcast",
    )
    add_idf_component(
        name="libbuffer",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/libbuffer",
    )
    add_idf_component(
        name="libmedian",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/libmedian",
    )
    add_idf_component(
        name="opus",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/opus",
    )
    add_idf_component(
        name="flac",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/flac",
    )
    add_idf_component(
        name="dsp_processor",
        ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
        repo="https://github.com/CarlosDerSeher/snapclient.git",
        path="components/dsp_processor",
    )
    if CONF_WEBSERVER_PORT in config:
        cg.add_build_flag(f"-DCONFIG_WEB_PORT={config[CONF_WEBSERVER_PORT]}")
        add_idf_component(
            name="ui_http_server",
            ref="38d749e6cc305cddfc62241fc60ffcde8056cf55",
            repo="https://github.com/CarlosDerSeher/snapclient.git",
            path="components/ui_http_server",
        )
    if (CONF_AUDIO_DAC not in config) or (CONF_WEBSERVER_PORT in config):
        add_idf_sdkconfig_option("CONFIG_USE_DSP_PROCESSOR", True)
        add_idf_sdkconfig_option("CONFIG_SNAPCLIENT_DSP_FLOW_STEREO", True)
    if CONF_AUDIO_DAC not in config:
        add_idf_sdkconfig_option("CONFIG_SNAPCLIENT_USE_SOFT_VOL", True)
    if CONF_NAME not in config:
        config[CONF_NAME] = CORE.name or ""
    # cg.add_build_flag("-DCONFIG_SNAPSERVER_HOST='"+str(config[CONF_HOSTNAME])+"'")
    # cg.add_build_flag("-DCONFIG_SNAPSERVER_PORT="+str(config[CONF_PORT]))
    if config[CONF_HOSTNAME] == 0:
        cg.add_build_flag("-DCONFIG_SNAPCLIENT_USE_MDNS=1")
    else:
        cg.add_build_flag("-DCONFIG_SNAPCLIENT_USE_MDNS=0")
    # cg.add_build_flag("-DCONFIG_SNAPCLIENT_NAME='"+config[CONF_NAME]+"'")
    cg.add_build_flag("-DCONFIG_USE_SAMPLE_INSERTION=1")
    # fix for esp-idf 5.4
    cg.add_build_flag("-Wno-error=incompatible-pointer-types")

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
