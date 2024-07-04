import hashlib
import logging
from pathlib import Path
import requests

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (
    __version__,
    CONF_ID,
    CONF_MODEL,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_TEMPERATURE_OFFSET,
    CONF_VOLTAGE,
)
from esphome import external_files

CODEOWNERS = ["@neffs"]

AUTO_LOAD = ["sensor", "text_sensor"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bme68x_bsec_i2c"

BSEC2_LIBRARY_VERSION = "v1.7.2502"

CONF_ALGORITHM_OUTPUT = "algorithm_output"
CONF_BME68X_BSEC_I2C_ID = "bme68x_bsec_i2c_id"
CONF_IAQ_MODE = "iaq_mode"
CONF_OPERATING_AGE = "operating_age"
CONF_STATE_SAVE_INTERVAL = "state_save_interval"

_LOGGER = logging.getLogger(__name__)

bme68x_bsec_i2c_ns = cg.esphome_ns.namespace("bme68x_bsec_i2c")

MODEL_OPTIONS = ["bme680", "bme688"]

AlgorithmOutput = bme68x_bsec_i2c_ns.enum("AlgorithmOutput")
ALGORITHM_OUTPUT_OPTIONS = {
    "classification": AlgorithmOutput.ALGORITHM_OUTPUT_CLASSIFICATION,
    "regression": AlgorithmOutput.ALGORITHM_OUTPUT_REGRESSION,
}

OperatingAge = bme68x_bsec_i2c_ns.enum("OperatingAge")
OPERATING_AGE_OPTIONS = {
    "4d": OperatingAge.OPERATING_AGE_4D,
    "28d": OperatingAge.OPERATING_AGE_28D,
}

SampleRate = bme68x_bsec_i2c_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "LP": SampleRate.SAMPLE_RATE_LP,
    "ULP": SampleRate.SAMPLE_RATE_ULP,
}

Voltage = bme68x_bsec_i2c_ns.enum("Voltage")
VOLTAGE_OPTIONS = {
    "1.8V": Voltage.VOLTAGE_1_8V,
    "3.3V": Voltage.VOLTAGE_3_3V,
}

BME68xBSECI2CComponent = bme68x_bsec_i2c_ns.class_(
    "BME68xBSECI2CComponent", cg.Component, i2c.I2CDevice
)

ALGORITHM_OUTPUT_FILE_NAME = {
    "classification": "sel",
    "regression": "reg",
}

SAMPLE_RATE_FILE_NAME = {
    "LP": "3s",
    "ULP": "300s",
}

VOLTAGE_FILE_NAME = {
    "1.8V": "18v",
    "3.3V": "33v",
}


def validate_bme68x(config):
    if CONF_ALGORITHM_OUTPUT not in config:
        return config

    if config[CONF_MODEL] != "bme688":
        raise cv.Invalid(f"{CONF_ALGORITHM_OUTPUT} is only valid for BME688")

    if config[CONF_ALGORITHM_OUTPUT] == "regression" and (
        config[CONF_OPERATING_AGE] != "4d"
        or config[CONF_SAMPLE_RATE] != "ULP"
        or config[CONF_VOLTAGE] != "1.8V"
    ):
        raise cv.Invalid(
            f" To use '{CONF_ALGORITHM_OUTPUT}: regression', {CONF_OPERATING_AGE} must be '4d', {CONF_SAMPLE_RATE} must be 'ULP' and {CONF_VOLTAGE} must be '1.8V'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME68xBSECI2CComponent),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Required(CONF_MODEL): cv.one_of(*MODEL_OPTIONS, lower=True),
            cv.Optional(CONF_ALGORITHM_OUTPUT): cv.enum(
                ALGORITHM_OUTPUT_OPTIONS, lower=True
            ),
            cv.Optional(CONF_OPERATING_AGE, default="28d"): cv.enum(
                OPERATING_AGE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_SAMPLE_RATE, default="LP"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_VOLTAGE, default="3.3V"): cv.enum(
                VOLTAGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature,
            cv.Optional(
                CONF_STATE_SAVE_INTERVAL, default="6hours"
            ): cv.positive_time_period_minutes,
        },
        cv.only_with_arduino,
    ).extend(i2c.i2c_device_schema(0x76)),
    validate_bme68x,
)


def _compute_url(config: dict) -> str:
    model = config.get(CONF_MODEL)
    operating_age = config.get(CONF_OPERATING_AGE)
    sample_rate = SAMPLE_RATE_FILE_NAME[config.get(CONF_SAMPLE_RATE)]
    volts = VOLTAGE_FILE_NAME[config.get(CONF_VOLTAGE)]
    algo = "iaq"
    filename = "bsec_iaq"
    if model == "bme688":
        algo = ALGORITHM_OUTPUT_FILE_NAME[
            config.get(CONF_ALGORITHM_OUTPUT, "classification")
        ]
        filename = "bsec_selectivity"
    return f"https://raw.githubusercontent.com/boschsensortec/Bosch-BSEC2-Library/{BSEC2_LIBRARY_VERSION}/src/config/{model}/{model}_{algo}_{volts}_{sample_rate}_{operating_age}/{filename}.txt"


def _compute_local_file_path(url: str) -> Path:
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _download_file(url: str, path: Path) -> bytes:
    if not external_files.has_remote_file_changed(url, path):
        _LOGGER.debug("Remote file has not changed, skipping download")
        return path.read_bytes()

    try:
        req = requests.get(
            url,
            timeout=external_files.NETWORK_TIMEOUT,
            headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
        )
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(f"Could not download file from {url}: {e}") from e

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(req.content)
    return req.content


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if algo_output := config.get(CONF_ALGORITHM_OUTPUT):
        cg.add(var.set_algorithm_output(algo_output))
    cg.add(var.set_operating_age(config[CONF_OPERATING_AGE]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_voltage(config[CONF_VOLTAGE]))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )

    url = _compute_url(config)
    path = _compute_local_file_path(url)
    bsec2_iaq_config = _download_file(url, path)

    # Convert retrieved BSEC config to an array of ints
    rhs = [int(x) for x in bsec2_iaq_config.decode("utf-8").split(",")]
    # Create an array which will reside in program memory and configure the sensor instance to use it
    bsec2_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.add(var.set_bsec_configuration(bsec2_arr, len(rhs)))

    # Although this component does not use SPI, the BSEC library requires the SPI library
    cg.add_library("SPI", None)
    cg.add_library(
        "BME68x Sensor library",
        "1.1.40407",
    )
    cg.add_library(
        "BSEC2 Software Library",
        None,
        f"https://github.com/boschsensortec/Bosch-BSEC2-Library.git#{BSEC2_LIBRARY_VERSION}",
    )

    cg.add_define("USE_BSEC2")
