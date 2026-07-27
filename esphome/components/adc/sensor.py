from dataclasses import dataclass
import logging

import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
from esphome.components.esp32 import (
    get_esp32_variant,
    include_builtin_idf_component,
    require_adc_oneshot_iram,
)
from esphome.components.nrf52.const import AIN_TO_GPIO, EXTRA_ADC
from esphome.components.zephyr import (
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_add_user,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.variants import VARIANTS
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_ID,
    CONF_NUMBER,
    CONF_PIN,
    CONF_RAW,
    DEVICE_CLASS_VOLTAGE,
    PLATFORM_NRF52,
    PLATFORM_ZEPHYR,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    PlatformFramework,
)
from esphome.core import CORE, ID, EsphomeError
from esphome.types import ConfigType

from . import (
    ATTENUATION_MODES,
    ESP32_VARIANT_ADC1_PIN_TO_CHANNEL,
    ESP32_VARIANT_ADC2_PIN_TO_CHANNEL,
    SAMPLING_MODES,
    adc_ns,
    adc_unit_t,
    validate_adc_pin,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["voltage_sampler"]

CONF_SAMPLES = "samples"
CONF_SAMPLING_MODE = "sampling_mode"
CONF_EMULATION = "emulation"


_attenuation = cv.enum(ATTENUATION_MODES, lower=True)
_sampling_mode = cv.enum(SAMPLING_MODES, lower=True)


def _validate_emulation_group(value):
    """A group is one sample() call's worth of readings: a scalar (sugar for a
    single-reading group) or a list of voltages, one per `samples:` reading."""
    if isinstance(value, list):
        return [round(cv.voltage(v) * 1000) for v in value]
    return [round(cv.voltage(value) * 1000)]


def _validate_emulation(value):
    if not isinstance(value, list) or not value:
        raise cv.Invalid(
            "emulation must be a non-empty list of voltage values or lists of voltage values"
        )
    if len(value) > 255:
        raise cv.Invalid(f"emulation supports at most 255 groups, got {len(value)}")
    return [_validate_emulation_group(entry) for entry in value]


# Only these four gains map to a real ESP32 attenuation setting; any other
# zephyr,gain value would return -ENOTSUP from adc_channel_setup() (adc_esp32.c).
ZEPHYR_ESP32_ATTENUATION_TO_GAIN = {
    "0db": "ADC_GAIN_1",
    "2.5db": "ADC_GAIN_4_5",
    "6db": "ADC_GAIN_1_2",
    "12db": "ADC_GAIN_1_4",
}


def validate_config(config):
    if emulation := config.get(CONF_EMULATION):
        sample_count = config.get(CONF_SAMPLES, 1)
        for group in emulation:
            if len(group) != sample_count:
                raise cv.Invalid(
                    f"Each `emulation` entry must supply exactly {sample_count} "
                    f"value(s) to match `samples: {sample_count}`"
                )

    if CONF_ATTENUATION in config and not (
        CORE.is_esp32
        or (CORE.is_zephyr and zephyr_variant_family() == "esp32")
        or CONF_EMULATION in config
    ):
        raise cv.Invalid(
            "attenuation is only valid on esp32, platform: zephyr with an "
            "esp32-family variant, or with `emulation:`",
            path=[CONF_ATTENUATION],
        )

    if config[CONF_RAW] and config.get(CONF_ATTENUATION, None) == "auto":
        raise cv.Invalid("Automatic attenuation cannot be used when raw output is set")

    if config.get(CONF_ATTENUATION, None) == "auto" and config.get(CONF_SAMPLES, 1) > 1:
        raise cv.Invalid(
            "Automatic attenuation cannot be used when multisampling is set"
        )
    if config.get(CONF_ATTENUATION) == "11db":
        _LOGGER.warning(
            "`attenuation: 11db` is deprecated, use `attenuation: 12db` instead"
        )
        # Alter value here so `config` command prints the recommended change
        config[CONF_ATTENUATION] = _attenuation("12db")

    return config


def _require_adc_iram(config: ConfigType) -> ConfigType:
    """Register ADC oneshot IRAM requirement during config validation."""
    if CORE.is_esp32:
        require_adc_oneshot_iram()
    return config


ADCSensor = adc_ns.class_(
    "ADCSensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONF_NRF_SAADC = "nrf_saadc"

adc_dt_spec = cg.global_ns.class_("adc_dt_spec")

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        ADCSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): validate_adc_pin,
            cv.Optional(CONF_RAW, default=False): cv.boolean,
            cv.SplitDefault(
                CONF_ATTENUATION,
                esp32="0db",
                zephyr_esp32="0db",
                zephyr_esp32h2="0db",
                zephyr_esp32c6="0db",
            ): _attenuation,
            cv.OnlyWith(CONF_NRF_SAADC, PLATFORM_NRF52): cv.declare_id(adc_dt_spec),
            cv.Optional(CONF_SAMPLES, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_SAMPLING_MODE, default="avg"): _sampling_mode,
            cv.Optional(CONF_EMULATION): cv.All(
                cv.only_on([PLATFORM_ZEPHYR]), _validate_emulation
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate_config,
    _require_adc_iram,
)


def _final_validate(config):
    if CONF_EMULATION in config:
        pin_value = config[CONF_PIN]
        if isinstance(pin_value, dict):
            pin_value = pin_value.get(CONF_NUMBER, pin_value)
        _LOGGER.warning(
            "'%s: %s' is ignored: 'emulation:' assigns the channel instead of the pin",
            CONF_PIN,
            pin_value,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

DOMAIN = "adc"


@dataclass
class ADCData:
    nrf52_channel_id: int = 0
    zephyr_emul_channel_id: int = 0
    zephyr_io_channel_index: int = 0


def _get_data() -> ADCData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ADCData()
    return CORE.data[DOMAIN]


def _next_zephyr_io_channel_index() -> int:
    """Return the next position in `zephyr_user`'s `io-channels` property.

    ADC_DT_SPEC_GET_BY_IDX() indexes by *append position* in that property,
    not by silicon channel number -- shared across every branch below that
    appends to it (esp32-family real hardware and `emulation:`) so the two
    can coexist in one config without colliding. nrf52 keeps its own separate
    counter (ADCData.nrf52_channel_id): it's a distinct target_platform that
    can never build alongside these branches, so there's no risk of collision.
    """
    data = _get_data()
    index = data.zephyr_io_channel_index
    data.zephyr_io_channel_index += 1
    return index


def _configure_zephyr_adc_channel(var, config: ConfigType, io_index: int) -> str:
    """Resolve `attenuation:` to a Zephyr `gain` string and wire up the shared adc_dt_spec Pvariable.

    Shared by the `emulation:` branch and the esp32-family real-hardware branch below: both index
    into the same zephyr_user `io-channels` property via `io_index` and reconfigure gain at runtime
    the same way when `attenuation: auto` is set.
    """
    attenuation = config.get(CONF_ATTENUATION, "0db")
    if attenuation == "auto":
        cg.add(var.set_autorange(True))
        # Devicetree default gain is only the initial channel config;
        # sample_autorange_() (adc_sensor_zephyr.cpp) reconfigures the
        # channel's gain per-read at runtime via adc_channel_setup().
        gain = ZEPHYR_ESP32_ATTENUATION_TO_GAIN["0db"]
    else:
        gain = ZEPHYR_ESP32_ATTENUATION_TO_GAIN[attenuation]

    adc_id = ID(f"{config[CONF_ID]}_adc_channel", is_declaration=True, type=adc_dt_spec)
    rhs = cg.RawExpression(f"ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), {io_index})")
    adc = cg.new_Pvariable(adc_id, rhs)
    cg.add(var.set_adc_channel(adc))
    return gain


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if CONF_EMULATION in config:
        # An emulated channel isn't wired to any real silicon pin -- `pin:` is still
        # required by schema but its value is ignored (see _final_validate's warning).
        pass
    elif config[CONF_PIN] == "VCC":
        cg.add_define("USE_ADC_SENSOR_VCC")
    elif config[CONF_PIN] == "TEMPERATURE":
        cg.add(var.set_is_temperature())
    elif not CORE.is_nrf52 or config[CONF_PIN][CONF_NUMBER] not in EXTRA_ADC:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))

    cg.add(var.set_output_raw(config[CONF_RAW]))
    cg.add(var.set_sample_count(config[CONF_SAMPLES]))
    cg.add(var.set_sampling_mode(config[CONF_SAMPLING_MODE]))

    if emulation := config.get(CONF_EMULATION):
        # Handled before the is_esp32/is_nrf52/zephyr-variant dispatch below: an emulated
        # channel works the same regardless of target chip, since adc_emul is generic.
        # emul_channel_id addresses the channel within zephyr_adc_emul's own namespace;
        # unrelated to io_index, the position in zephyr_user's shared `io-channels`.
        data = _get_data()
        emul_channel_id = data.zephyr_emul_channel_id
        data.zephyr_emul_channel_id += 1
        io_index = _next_zephyr_io_channel_index()

        zephyr_add_prj_conf("ADC", True)

        gain = _configure_zephyr_adc_channel(var, config, io_index)
        zephyr_add_user("io-channels", f"<&zephyr_adc_emul {emul_channel_id}>")
        # Re-declaring the zephyr_adc_emul node per channel is idempotent: dtc merges
        # fragments targeting the same path, scalars take the last value, and each
        # channel@N child is additive -- same pattern esp32-family/nrf52 rely on below.
        zephyr_add_overlay(
            f"""
                / {{
                    zephyr_adc_emul: adc_emul {{
                        compatible = "zephyr,adc-emul";
                        #io-channel-cells = <1>;
                        nchannels = <{emul_channel_id + 1}>;
                        ref-internal-mv = <3300>;
                        status = "okay";
                    }};
                }};

                &zephyr_adc_emul {{
                    #address-cells = <1>;
                    #size-cells = <0>;

                    channel@{emul_channel_id} {{
                        reg = <{emul_channel_id}>;
                        zephyr,gain = "{gain}";
                        zephyr,reference = "ADC_REF_INTERNAL";
                        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
                        zephyr,resolution = <12>;
                    }};
                }};
            """
        )

        flat_values = [v for group in emulation for v in group]
        arr_id = ID(
            f"{config[CONF_ID]}_emulation_values",
            is_declaration=True,
            type=cg.uint32,
        )
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*flat_values))
        cg.add_define("USE_ZEPHYR_ADC_EMULATION")
        cg.add(var.set_emulated_values(arr, config[CONF_SAMPLES], len(emulation)))

    elif CORE.is_esp32:
        # Re-enable ESP-IDF's ADC driver (excluded by default to save compile time)
        include_builtin_idf_component("esp_adc")

        if attenuation := config.get(CONF_ATTENUATION):
            if attenuation == "auto":
                cg.add(var.set_autorange(cg.global_ns.true))
            else:
                cg.add(var.set_attenuation(attenuation))

        variant = get_esp32_variant()
        pin_num = config[CONF_PIN][CONF_NUMBER]
        if (
            variant in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL
            and pin_num in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant]
        ):
            chan = ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant][pin_num]
            cg.add(var.set_channel(adc_unit_t.ADC_UNIT_1, chan))
        elif (
            variant in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL
            and pin_num in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant]
        ):
            chan = ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant][pin_num]
            cg.add(var.set_channel(adc_unit_t.ADC_UNIT_2, chan))

    elif CORE.is_nrf52:
        data = _get_data()
        channel_id = data.nrf52_channel_id
        data.nrf52_channel_id += 1
        zephyr_add_prj_conf("ADC", True)
        nrf_saadc = config[CONF_NRF_SAADC]
        rhs = cg.RawExpression(
            f"ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), {channel_id})"
        )
        adc = cg.new_Pvariable(nrf_saadc, rhs)
        cg.add(var.set_adc_channel(adc))
        gain = "ADC_GAIN_1_6"
        pin_number = config[CONF_PIN][CONF_NUMBER]
        if pin_number == "VDDHDIV5":
            gain = "ADC_GAIN_1_2"
        if isinstance(pin_number, int):
            GPIO_TO_AIN = {v: k for k, v in AIN_TO_GPIO.items()}
            pin_number = GPIO_TO_AIN[pin_number]
        zephyr_add_user("io-channels", f"<&adc {channel_id}>")
        zephyr_add_overlay(
            f"""
                &adc {{
                    #address-cells = <1>;
                    #size-cells = <0>;

                    channel@{channel_id} {{
                        reg = <{channel_id}>;
                        zephyr,gain = "{gain}";
                        zephyr,reference = "ADC_REF_INTERNAL";
                        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
                        zephyr,input-positive = <NRF_SAADC_{pin_number}>;
                        zephyr,resolution = <14>;
                        zephyr,oversampling = <8>;
                    }};
                }};
            """
        )
    elif CORE.using_zephyr and zephyr_variant_family() == "esp32":
        variant = zephyr_variant()
        pin_num = config[CONF_PIN][CONF_NUMBER]
        channel_map = VARIANTS[variant].adc1_channel_map
        if pin_num not in channel_map:
            raise EsphomeError(f"Pin {pin_num} is not a valid ADC1 pin on {variant}")
        # channel_id addresses the channel within adc0's own silicon namespace; unrelated
        # to io_index, the position in zephyr_user's shared `io-channels` (shared counter
        # with the `emulation:` branch above so the two can coexist in one config).
        channel_id = channel_map[pin_num]
        io_index = _next_zephyr_io_channel_index()
        zephyr_add_prj_conf("ADC", True)

        gain = _configure_zephyr_adc_channel(var, config, io_index)
        zephyr_add_user("io-channels", f"<&adc0 {channel_id}>")
        zephyr_add_overlay(
            f"""
                &adc0 {{
                    status = "okay";
                    #address-cells = <1>;
                    #size-cells = <0>;

                    channel@{channel_id} {{
                        reg = <{channel_id}>;
                        zephyr,gain = "{gain}";
                        zephyr,reference = "ADC_REF_INTERNAL";
                        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
                        zephyr,resolution = <12>;
                    }};
                }};
            """
        )
    elif CORE.using_zephyr:
        raise EsphomeError(
            f"ADC is not yet implemented for Zephyr variant '{zephyr_variant()}'"
        )


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "adc_sensor_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "adc_sensor_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "adc_sensor_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
        "adc_sensor_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "adc_sensor_zephyr.cpp": {
            PlatformFramework.NRF52_ZEPHYR,
            PlatformFramework.ZEPHYR_ZEPHYR,
        },
    }
)
