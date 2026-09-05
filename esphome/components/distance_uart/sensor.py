from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_MAX_RANGE,
    CONF_MODE,
    CONF_MODEL,
    CONF_TRIGGER_PIN,
    CONF_UART_ID,
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.core import CORE, EsphomeError

# This component depends on the UART component.
DEPENDENCIES = ["uart"]

# Create a namespace for our C++ code
distance_uart_ns = cg.esphome_ns.namespace("distance_uart")
DistanceUARTSensor = distance_uart_ns.class_(
    "DistanceUARTSensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
DistanceUARTMode = distance_uart_ns.enum("DistanceUARTMode")
DistanceUARTOutputMode = distance_uart_ns.enum("DistanceUARTOutputMode")
DistanceUARTPublishMode = distance_uart_ns.enum("DistanceUARTPublishMode")

# Define new configuration keys and modes
CONF_BLIND_ZONE = "blind_zone"
CONF_OUTPUT_MODE = "output_mode"
CONF_OUTPUT_MODE_PIN = "output_mode_pin"
CONF_PUBLISH_MODE = "publish_mode"

MODES = {
    "CONTROLLED": DistanceUARTMode.MODE_CONTROLLED,
    "AUTO": DistanceUARTMode.MODE_AUTO,
}
OUTPUT_MODES = {
    "PROCESSED": DistanceUARTOutputMode.OUTPUT_MODE_PROCESSED,
    "REALTIME": DistanceUARTOutputMode.OUTPUT_MODE_REALTIME,
}
PUBLISH_MODES = {
    "INTERVAL": DistanceUARTPublishMode.PUBLISH_MODE_INTERVAL,
    "IMMEDIATE": DistanceUARTPublishMode.PUBLISH_MODE_IMMEDIATE,
}

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


def _validate_user_config(config):
    if CONF_MODEL in config:
        model_conf = KNOWN_MODELS[config[CONF_MODEL]]
        if "mode" in model_conf and CONF_MODE in config:
            raise cv.Invalid(
                f"You cannot manually specify 'mode' when using model '{config[CONF_MODEL]}', as it already defines the mode."
            )
        if CONF_BLIND_ZONE in config or CONF_MAX_RANGE in config:
            raise cv.Invalid(
                f"You cannot manually specify 'blind_zone' or 'max_range' when using model '{config[CONF_MODEL]}'."
            )
        if not model_conf.get("supports_rx_mode_select", False) and (
            CONF_OUTPUT_MODE in config or CONF_OUTPUT_MODE_PIN in config
        ):
            raise cv.Invalid(
                f"Model '{config[CONF_MODEL]}' does not support 'output_mode' or 'output_mode_pin'."
            )

    effective_mode_str = config.get(CONF_MODE, "AUTO")
    if CONF_MODEL in config:
        model_conf = KNOWN_MODELS[config[CONF_MODEL]]
        if "mode" in model_conf:
            effective_mode_str = model_conf["mode"]

    if effective_mode_str.upper() == "AUTO":
        if CONF_TRIGGER_PIN in config:
            raise cv.Invalid("`trigger_pin` is not allowed when `mode` is 'AUTO'.")
    elif effective_mode_str.upper() == "CONTROLLED" and (
        CONF_OUTPUT_MODE in config or CONF_OUTPUT_MODE_PIN in config
    ):
        raise cv.Invalid(
            "'output_mode' and 'output_mode_pin' are only allowed when 'mode' is 'AUTO'."
        )

    return config


CONFIG_SCHEMA = cv.All(
    _validate_user_config,
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
            cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_MODE, default="AUTO"): cv.enum(MODES, upper=True),
            cv.Optional(CONF_MODEL): cv.one_of(*KNOWN_MODELS, upper=True),
            cv.Optional(CONF_BLIND_ZONE): cv.distance,
            cv.Optional(CONF_MAX_RANGE): cv.distance,
            cv.Optional(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OUTPUT_MODE, default="PROCESSED"): cv.enum(
                OUTPUT_MODES, upper=True
            ),
            cv.Optional(CONF_OUTPUT_MODE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PUBLISH_MODE, default="INTERVAL"): cv.enum(
                PUBLISH_MODES, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("1s")),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "distance_uart",
    require_tx=False,
    require_rx=True,
    baud_rate=None,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await uart.register_uart_device(var, config)

    uart_conf = None
    if CONF_UART_ID in config:
        uart_id_to_find = str(config[CONF_UART_ID])
        if "uart" in CORE.config:
            for conf in CORE.config["uart"]:
                if str(conf[CONF_ID]) == uart_id_to_find:
                    uart_conf = conf
                    break
    # Fallback to the first available UART if CONF_UART_ID is missing
    elif "uart" in CORE.config and len(CORE.config["uart"]) > 0:
        uart_conf = CORE.config["uart"][0]

    if uart_conf is None:
        raise EsphomeError(
            "Could not find any UART component. Please ensure it is defined in your YAML."
        )

    final_baud_rate = uart_conf.get(CONF_BAUD_RATE, 9600)
    uart_id_to_find = str(uart_conf[CONF_ID])

    # Check baud rate if model is specified
    if CONF_MODEL in config:
        model_conf = KNOWN_MODELS[config[CONF_MODEL]]
        required_baud = model_conf.get("baud_rate", 9600)

        if final_baud_rate != required_baud:
            raise EsphomeError(
                f"Model '{config[CONF_MODEL]}' requires a baud_rate of {required_baud}, "
                f"but the connected UART component '{uart_id_to_find}' is configured for {final_baud_rate}. "
                f"Please adjust your UART configuration."
            )

    final_blind_zone = config.get(CONF_BLIND_ZONE)
    final_max_range = config.get(CONF_MAX_RANGE)
    final_mode_enum = config[CONF_MODE]
    final_publish_mode_enum = config[CONF_PUBLISH_MODE]

    if CONF_MODEL in config:
        model_conf = KNOWN_MODELS[config[CONF_MODEL]]
        final_blind_zone = cv.distance(model_conf["blind_zone"])
        final_max_range = cv.distance(model_conf["max_range"])
        # Use the validated baud rate from above or default
        if "mode" in model_conf:
            mode_validator = cv.enum(MODES, upper=True)
            final_mode_enum = mode_validator(model_conf["mode"])

    if final_blind_zone is not None:
        cg.add(var.set_blind_zone(final_blind_zone))
    else:
        cg.add(var.set_blind_zone(0.0))

    if final_max_range is not None:
        cg.add(var.set_max_range(final_max_range))

    cg.add(var.set_mode(final_mode_enum))
    cg.add(var.set_publish_mode(final_publish_mode_enum))
    cg.add(var.set_baud_rate(final_baud_rate))

    if final_mode_enum == MODES["CONTROLLED"]:
        if CONF_TRIGGER_PIN in config:
            trigger_pin_obj = await cg.gpio_pin_expression(config[CONF_TRIGGER_PIN])
            cg.add(var.set_trigger_pin(trigger_pin_obj))
        elif uart_conf is None or "tx_pin" not in uart_conf:
            raise EsphomeError(
                f"distance_uart sensor in CONTROLLED mode has no 'trigger_pin' and the parent uart "
                f"component '{uart_id_to_find}' does not have a 'tx_pin' defined."
            )

    elif final_mode_enum == MODES["AUTO"]:
        if CONF_OUTPUT_MODE_PIN in config:
            output_mode_pin_obj = await cg.gpio_pin_expression(
                config[CONF_OUTPUT_MODE_PIN]
            )
            cg.add(var.set_output_mode_pin(output_mode_pin_obj))
            cg.add(var.set_output_mode(config[CONF_OUTPUT_MODE]))
        elif CONF_OUTPUT_MODE in config and config[CONF_OUTPUT_MODE].is_set:
            if uart_conf is None or "tx_pin" not in uart_conf:
                raise EsphomeError(
                    f"distance_uart sensor in AUTO mode has 'output_mode' set but no 'output_mode_pin', and the parent uart "
                    f"component '{uart_id_to_find}' does not have a 'tx_pin' for fallback."
                )
            output_mode_pin_obj = await cg.gpio_pin_expression(uart_conf["tx_pin"])
            cg.add(var.set_output_mode_pin(output_mode_pin_obj))
            cg.add(var.set_output_mode(config[CONF_OUTPUT_MODE]))
