import hashlib
from pathlib import Path

from esphome import core, external_files
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_TEMPERATURE_OFFSET,
)

CODEOWNERS = ["@neffs", "@kbx81"]

DOMAIN = "bme68x_bsec2"

BSEC2_LIBRARY_VERSION = "v1.7.2502"

CONF_ALGORITHM_OUTPUT = "algorithm_output"
CONF_BME68X_BSEC2_ID = "bme68x_bsec2_id"
CONF_IAQ_MODE = "iaq_mode"
CONF_OPERATING_AGE = "operating_age"
CONF_STATE_SAVE_INTERVAL = "state_save_interval"
CONF_SUPPLY_VOLTAGE = "supply_voltage"

bme68x_bsec2_ns = cg.esphome_ns.namespace("bme68x_bsec2")
BME68xBSEC2Component = bme68x_bsec2_ns.class_("BME68xBSEC2Component", cg.Component)


MODEL_OPTIONS = ["bme680", "bme688"]

AlgorithmOutput = bme68x_bsec2_ns.enum("AlgorithmOutput")
ALGORITHM_OUTPUT_OPTIONS = {
    "classification": AlgorithmOutput.ALGORITHM_OUTPUT_CLASSIFICATION,
    "regression": AlgorithmOutput.ALGORITHM_OUTPUT_REGRESSION,
}

OperatingAge = bme68x_bsec2_ns.enum("OperatingAge")
OPERATING_AGE_OPTIONS = {
    "4d": OperatingAge.OPERATING_AGE_4D,
    "28d": OperatingAge.OPERATING_AGE_28D,
}

SampleRate = bme68x_bsec2_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "LP": SampleRate.SAMPLE_RATE_LP,
    "ULP": SampleRate.SAMPLE_RATE_ULP,
}

Voltage = bme68x_bsec2_ns.enum("Voltage")
VOLTAGE_OPTIONS = {
    "1.8V": Voltage.VOLTAGE_1_8V,
    "3.3V": Voltage.VOLTAGE_3_3V,
}

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


def _compute_local_file_path(url: str) -> Path:
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _compute_url(config: dict) -> str:
    model = config.get(CONF_MODEL)
    operating_age = config.get(CONF_OPERATING_AGE)
    sample_rate = SAMPLE_RATE_FILE_NAME[config.get(CONF_SAMPLE_RATE)]
    volts = VOLTAGE_FILE_NAME[config.get(CONF_SUPPLY_VOLTAGE)]
    if model == "bme688":
        algo = ALGORITHM_OUTPUT_FILE_NAME[
            config.get(CONF_ALGORITHM_OUTPUT, "classification")
        ]
        filename = "bsec_selectivity"
    else:
        algo = "iaq"
        filename = "bsec_iaq"
    return f"https://raw.githubusercontent.com/boschsensortec/Bosch-BSEC2-Library/{BSEC2_LIBRARY_VERSION}/src/config/{model}/{model}_{algo}_{volts}_{sample_rate}_{operating_age}/{filename}.txt"


def download_bme68x_blob(config):
    url = _compute_url(config)
    path = _compute_local_file_path(url)
    external_files.download_content(url, path)

    return config


def validate_bme68x(config):
    if CONF_ALGORITHM_OUTPUT not in config:
        return config

    if config[CONF_MODEL] != "bme688":
        raise cv.Invalid(f"{CONF_ALGORITHM_OUTPUT} is only valid for BME688")

    if config[CONF_ALGORITHM_OUTPUT] == "regression" and (
        config[CONF_OPERATING_AGE] != "4d"
        or config[CONF_SAMPLE_RATE] != "ULP"
        or config[CONF_SUPPLY_VOLTAGE] != "1.8V"
    ):
        raise cv.Invalid(
            f" To use '{CONF_ALGORITHM_OUTPUT}: regression', {CONF_OPERATING_AGE} must be '4d', {CONF_SAMPLE_RATE} must be 'ULP' and {CONF_SUPPLY_VOLTAGE} must be '1.8V'"
        )
    return config


CONFIG_SCHEMA_BASE = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME68xBSEC2Component),
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
            cv.Optional(CONF_SUPPLY_VOLTAGE, default="3.3V"): cv.enum(
                VOLTAGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature,
            cv.Optional(
                CONF_STATE_SAVE_INTERVAL, default="6hours"
            ): cv.positive_time_period_minutes,
        },
    )
    .add_extra(cv.only_with_arduino)
    .add_extra(validate_bme68x)
    .add_extra(download_bme68x_blob)
)


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if algo_output := config.get(CONF_ALGORITHM_OUTPUT):
        cg.add(var.set_algorithm_output(algo_output))
    cg.add(var.set_operating_age(config[CONF_OPERATING_AGE]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_voltage(config[CONF_SUPPLY_VOLTAGE]))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )

    path = _compute_local_file_path(_compute_url(config))

    try:
        with open(path, encoding="utf-8") as f:
            bsec2_iaq_config = f.read()
    except Exception as e:
        raise core.EsphomeError(f"Could not open binary configuration file {path}: {e}")

    # Convert retrieved BSEC2 config to an array of ints
    rhs = [int(x) for x in bsec2_iaq_config.split(",")]
    # Create an array which will reside in program memory and configure the sensor instance to use it
    bsec2_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.add(var.set_bsec2_configuration(bsec2_arr, len(rhs)))

    # Although this component does not use SPI, the BSEC2 library requires the SPI library
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

    return var
