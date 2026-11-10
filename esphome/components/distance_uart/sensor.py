from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.const import CONF_OUTPUT_MODE
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_RANGE,
    CONF_MODE,
    CONF_MODEL,
    CONF_TRIGGER_PIN,
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

distance_uart_ns = cg.esphome_ns.namespace("distance_uart")
DistanceUARTSensor = distance_uart_ns.class_(
    "DistanceUARTSensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
DistanceUARTMode = distance_uart_ns.enum("DistanceUARTMode", is_class=True)
DistanceUARTOutputMode = distance_uart_ns.enum("DistanceUARTOutputMode", is_class=True)
DistanceUARTPublishMode = distance_uart_ns.enum(
    "DistanceUARTPublishMode", is_class=True
)

CONF_BLIND_ZONE = "blind_zone"
CONF_OUTPUT_MODE_PIN = "output_mode_pin"
CONF_PUBLISH_MODE = "publish_mode"

MODES = DistanceUARTMode.options("CONTROLLED", "AUTO", prefix="DISTANCE_UART_MODE_")
OUTPUT_MODES = DistanceUARTOutputMode.options(
    "PROCESSED", "REALTIME", prefix="DISTANCE_UART_OUTPUT_MODE_"
)
PUBLISH_MODES = DistanceUARTPublishMode.options(
    "INTERVAL", "IMMEDIATE", prefix="DISTANCE_UART_PUBLISH_MODE_"
)

# Define a dictionary of known models and their properties.
# fmt: off
KNOWN_MODELS = {
    # A01 Models
    "A01A": {"blind_zone": "28cm", "max_range": "750cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01ANYTB": {"blind_zone": "28cm", "max_range": "750cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A01ANYUB": {"blind_zone": "28cm", "max_range": "750cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01B": {"blind_zone": "28cm", "max_range": "750cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01BNYTB": {"blind_zone": "28cm", "max_range": "750cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A01BNYUB": {"blind_zone": "28cm", "max_range": "750cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01BNYTW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A01BNYUW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01C": {"blind_zone": "28cm", "max_range": "250cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A01CNYTB": {"blind_zone": "28cm", "max_range": "250cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A01CNYUB": {"blind_zone": "28cm", "max_range": "250cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A02 Models
    "A02": {"blind_zone": "3cm", "max_range": "450cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A02YYT": {"blind_zone": "3cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A02YYTW": {"blind_zone": "3cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A02YYU": {"blind_zone": "3cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A02YYUW": {"blind_zone": "3cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A06 Models
    "A06B": {"blind_zone": "30cm", "max_range": "200cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A06BLYT": {"blind_zone": "30cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A06BLYU": {"blind_zone": "30cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A06BNYT": {"blind_zone": "30cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A06BNYU": {"blind_zone": "30cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A06LYT": {"blind_zone": "25cm", "max_range": "600cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A06LYU": {"blind_zone": "25cm", "max_range": "600cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A06NYT": {"blind_zone": "25cm", "max_range": "600cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A06NYU": {"blind_zone": "25cm", "max_range": "600cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A07 Models
    "A07": {"blind_zone": "25cm", "max_range": "800cm", "supports_rx_mode_select": False, "baud_rate": 9600},
    "A07NYTB": {"blind_zone": "25cm", "max_range": "800cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A07NYUB": {"blind_zone": "25cm", "max_range": "800cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # A08 Models
    "A08A": {"blind_zone": "25cm", "max_range": "800cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A08ANYTB": {"blind_zone": "25cm", "max_range": "800cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A08ANYUB": {"blind_zone": "25cm", "max_range": "800cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A08B": {"blind_zone": "25cm", "max_range": "500cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A08BNYTB": {"blind_zone": "25cm", "max_range": "500cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A08BNYUB": {"blind_zone": "25cm", "max_range": "500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A08C": {"blind_zone": "25cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A08CNYUB": {"blind_zone": "25cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A09 Models
    "A09A": {"blind_zone": "20cm", "max_range": "350cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A09ANYTW": {"blind_zone": "20cm", "max_range": "350cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A09ANYUW": {"blind_zone": "20cm", "max_range": "350cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A09B": {"blind_zone": "28cm", "max_range": "350cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A09BNYTW": {"blind_zone": "28cm", "max_range": "350cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A09BNYUW": {"blind_zone": "28cm", "max_range": "350cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A10 Models
    "A10A": {"blind_zone": "25cm", "max_range": "450cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A10ANYTW": {"blind_zone": "25cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A10ANYUW": {"blind_zone": "25cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A10B": {"blind_zone": "28cm", "max_range": "450cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A10BNYTW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A10BNYUW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A11 Models
    "A11A": {"blind_zone": "21cm", "max_range": "300cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A11ANYTW": {"blind_zone": "21cm", "max_range": "300cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A11ANYUW": {"blind_zone": "21cm", "max_range": "300cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A11B": {"blind_zone": "23cm", "max_range": "200cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A11BNYTW": {"blind_zone": "23cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A11BNYUW": {"blind_zone": "23cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A12 Models
    "A12A": {"blind_zone": "25cm", "max_range": "500cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A12ANYTW": {"blind_zone": "25cm", "max_range": "500cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A12ANYUW": {"blind_zone": "25cm", "max_range": "500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A12B": {"blind_zone": "25cm", "max_range": "500cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A12BNYTW": {"blind_zone": "25cm", "max_range": "500cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A12BNYUW": {"blind_zone": "25cm", "max_range": "500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A13 Models
    "A13": {"blind_zone": "25cm", "max_range": "200cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A13B": {"blind_zone": "25cm", "max_range": "200cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A13BNYTW": {"blind_zone": "25cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A13BNYUW": {"blind_zone": "25cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A13NYTW": {"blind_zone": "25cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A13NYUW": {"blind_zone": "25cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A15 Models
    "A15": {"blind_zone": "15cm", "max_range": "200cm", "supports_rx_mode_select": False, "baud_rate": 9600},
    "A15NYTW": {"blind_zone": "15cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A15NYUW": {"blind_zone": "15cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # A16 Models
    "A16": {"blind_zone": "50cm", "max_range": "1500cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A16NYTW": {"blind_zone": "50cm", "max_range": "1500cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A16NYUW": {"blind_zone": "50cm", "max_range": "1500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A17 Models
    "A17": {"blind_zone": "25cm", "max_range": "1000cm", "supports_rx_mode_select": False, "baud_rate": 9600},
    "A17NYTW": {"blind_zone": "25cm", "max_range": "1000cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A17NYUW": {"blind_zone": "25cm", "max_range": "1000cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # A19 Models
    "A19": {"blind_zone": "28cm", "max_range": "450cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A19NYTW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A19NYUW": {"blind_zone": "28cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A20 Models
    "A20": {"blind_zone": "3cm", "max_range": "300cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "A20NYTW": {"blind_zone": "3cm", "max_range": "300cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "A20NYUW": {"blind_zone": "3cm", "max_range": "300cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    # A21 Models
    "A21A": {"blind_zone": "3cm", "max_range": "500cm", "supports_rx_mode_select": True, "baud_rate": 115200},
    "A21AYYTW": {"blind_zone": "3cm", "max_range": "500cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "A21AYYUW": {"blind_zone": "3cm", "max_range": "500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 115200},
    "A21B": {"blind_zone": "3cm", "max_range": "500cm", "supports_rx_mode_select": True, "baud_rate": 115200},
    "A21BYYTW": {"blind_zone": "3cm", "max_range": "500cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "A21BYYUW": {"blind_zone": "3cm", "max_range": "500cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 115200},
    # A22 Models
    "A22": {"blind_zone": "2cm", "max_range": "300cm", "supports_rx_mode_select": True, "baud_rate": 115200},
    "A22NYTW": {"blind_zone": "2cm", "max_range": "300cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "A22NYUW": {"blind_zone": "2cm", "max_range": "300cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 115200},
    # A25 Models
    "A25": {"blind_zone": "3cm", "max_range": "200cm", "supports_rx_mode_select": True, "baud_rate": 115200},
    "A25YYTW": {"blind_zone": "3cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "A25YYUW": {"blind_zone": "3cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 115200},
    # DS1603 Models
    "DS1603DA-3U": {"blind_zone": "4cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    "DS1603L": {"blind_zone": "5cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # L02 Models
    "L02": {"blind_zone": "2cm", "max_range": "200cm", "supports_rx_mode_select": False, "baud_rate": 9600},
    "L023MTW": {"blind_zone": "2cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "L023MUW": {"blind_zone": "2cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # L07A Models
    "L07A": {"blind_zone": "1.5cm", "max_range": "200cm", "supports_rx_mode_select": False, "baud_rate": 9600},
    "L07AYYTW": {"blind_zone": "1.5cm", "max_range": "200cm", "mode": "CONTROLLED", "baud_rate": 9600},
    "L07AYYUW": {"blind_zone": "1.5cm", "max_range": "200cm", "mode": "AUTO", "supports_rx_mode_select": False, "baud_rate": 9600},
    # L08 Models
    "L08": {"blind_zone": "5cm", "max_range": "1000cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "L081MTW": {"blind_zone": "5cm", "max_range": "1000cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "L08B": {"blind_zone": "8cm", "max_range": "1000cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "L08B50TW": {"blind_zone": "8cm", "max_range": "1000cm", "mode": "CONTROLLED", "baud_rate": 115200},
    # ME007YS Models
    "ME007YS": {"blind_zone": "28cm", "max_range": "450cm", "supports_rx_mode_select": True, "baud_rate": 9600},
    "ME007YS-TX": {"blind_zone": "28cm", "max_range": "450cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 9600},
    "ME007YS-TX1": {"blind_zone": "28cm", "max_range": "450cm", "mode": "CONTROLLED", "baud_rate": 9600},
    # R01 Models
    "R01": {"blind_zone": "2cm", "max_range": "400cm", "supports_rx_mode_select": True, "baud_rate": 115200},
    "R01TW": {"blind_zone": "2cm", "max_range": "400cm", "mode": "CONTROLLED", "baud_rate": 115200},
    "R01UW": {"blind_zone": "2cm", "max_range": "400cm", "mode": "AUTO", "supports_rx_mode_select": True, "baud_rate": 115200},
}
# fmt: on


def _get_effective_mode(config: ConfigType) -> str:
    if (model := config.get(CONF_MODEL)) is not None and (
        mode := KNOWN_MODELS[model].get("mode")
    ) is not None:
        return mode
    return str(config.get(CONF_MODE, "AUTO"))


def _validate_user_config(config: ConfigType) -> ConfigType:
    if (model := config.get(CONF_MODEL)) is not None:
        model_conf = KNOWN_MODELS[model]
        if "mode" in model_conf and CONF_MODE in config:
            raise cv.Invalid(
                f"You cannot manually specify 'mode' when using model '{model}', as it already defines the mode."
            )
        if CONF_BLIND_ZONE in config or CONF_MAX_RANGE in config:
            raise cv.Invalid(
                f"You cannot manually specify 'blind_zone' or 'max_range' when using model '{model}'."
            )
        if not model_conf.get("supports_rx_mode_select", False) and (
            CONF_OUTPUT_MODE in config or CONF_OUTPUT_MODE_PIN in config
        ):
            raise cv.Invalid(
                f"Model '{model}' does not support 'output_mode' or 'output_mode_pin'."
            )

    effective_mode = _get_effective_mode(config)
    if effective_mode == "AUTO":
        if CONF_TRIGGER_PIN in config:
            raise cv.Invalid("`trigger_pin` is not allowed when `mode` is 'AUTO'.")
        if CONF_OUTPUT_MODE in config and CONF_OUTPUT_MODE_PIN not in config:
            raise cv.Invalid(
                "'output_mode' requires 'output_mode_pin' to be configured."
            )
    elif effective_mode == "CONTROLLED" and (
        CONF_OUTPUT_MODE in config or CONF_OUTPUT_MODE_PIN in config
    ):
        raise cv.Invalid(
            "'output_mode' and 'output_mode_pin' are only allowed when 'mode' is 'AUTO'."
        )

    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DistanceUARTSensor,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_MODE): cv.enum(MODES, upper=True),
            cv.Optional(CONF_MODEL): cv.one_of(*KNOWN_MODELS, upper=True),
            cv.Optional(CONF_BLIND_ZONE): cv.distance,
            cv.Optional(CONF_MAX_RANGE): cv.distance,
            cv.Optional(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OUTPUT_MODE): cv.enum(OUTPUT_MODES, upper=True),
            cv.Optional(CONF_OUTPUT_MODE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PUBLISH_MODE, default="INTERVAL"): cv.enum(
                PUBLISH_MODES, upper=True
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("1s")),
    _validate_user_config,
)


def final_validate_config(config: ConfigType) -> ConfigType:
    effective_mode = _get_effective_mode(config)
    require_tx = effective_mode == "CONTROLLED" and CONF_TRIGGER_PIN not in config
    baud_rate = (
        KNOWN_MODELS[model]["baud_rate"]
        if (model := config.get(CONF_MODEL)) is not None
        else None
    )

    return uart.final_validate_device_schema(
        "distance_uart",
        require_tx=require_tx,
        require_rx=True,
        baud_rate=baud_rate,
        data_bits=8,
        parity=None,
        stop_bits=1,
    )(config)


FINAL_VALIDATE_SCHEMA = final_validate_config


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    await sensor.register_sensor(var, config)

    effective_mode = _get_effective_mode(config)
    if (model := config.get(CONF_MODEL)) is not None:
        model_conf = KNOWN_MODELS[model]
        blind_zone = cv.distance(model_conf["blind_zone"])
        max_range = cv.distance(model_conf["max_range"])
    else:
        blind_zone = config.get(CONF_BLIND_ZONE)
        max_range = config.get(CONF_MAX_RANGE)

    cg.add_optional(var.set_blind_zone, blind_zone)
    cg.add_optional(var.set_max_range, max_range)
    cg.add(var.set_mode(MODES[effective_mode]))
    cg.add(var.set_publish_mode(config[CONF_PUBLISH_MODE]))

    if effective_mode == "CONTROLLED":
        await cg.add_pin(var.set_trigger_pin, config.get(CONF_TRIGGER_PIN))
    elif effective_mode == "AUTO":
        await cg.add_pin(var.set_output_mode_pin, config.get(CONF_OUTPUT_MODE_PIN))
        cg.add_optional(var.set_output_mode, config.get(CONF_OUTPUT_MODE))
