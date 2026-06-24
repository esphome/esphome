import pkgutil
import importlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.config_validation import update_interval
from esphome import pins as esphome_pins
from esphome.components import i2c, display
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import ID

from . import models

DEPENDENCIES = ["i2c"]

CONF_PCA6416A_ID   = "pca6416a_id"
CONF_MCP23017_ID   = "mcp23017_id"
CONF_PIN_CKV       = "pin_ckv"
CONF_PIN_SPH       = "pin_sph"
CONF_PIN_LE        = "pin_le"
CONF_GRAYSCALE_MODE = "grayscale_mode"

# Auto-load every model file so they self-register into InkplateParallelModel.models
for _mod in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{_mod.name}", package=__package__)

MODELS = models.InkplateParallelModel.models

inkplate_ns      = cg.esphome_ns.namespace("inkplate")
pca6416a_ns      = cg.esphome_ns.namespace("pca6416a")
mcp23017_ns      = cg.esphome_ns.namespace("mcp23017")
mcp23xxx_base_ns = cg.esphome_ns.namespace("mcp23xxx_base")

PCA6416AComponent = pca6416a_ns.class_("PCA6416AComponent")
PCA6416AGPIOPin   = pca6416a_ns.class_("PCA6416AGPIOPin", cg.GPIOPin)
MCP23017Component = mcp23017_ns.class_("MCP23017")
MCP23017GPIOPin   = mcp23xxx_base_ns.class_("MCP23XXXGPIOPin<16>", cg.GPIOPin)

_CPP_CLASSES = {
    name: inkplate_ns.class_(model.cpp_class, display.Display, i2c.I2CDevice)
    for name, model in MODELS.items()
}

_DIRECT_PIN_CONF = {
    "ckv": CONF_PIN_CKV,
    "sph": CONF_PIN_SPH,
    "le":  CONF_PIN_LE,
}


def _validate_expander(config):
    has_pca = CONF_PCA6416A_ID in config
    has_mcp = CONF_MCP23017_ID in config
    if not has_pca and not has_mcp:
        raise cv.Invalid("One of 'pca6416a_id' or 'mcp23017_id' must be specified")
    if has_pca and has_mcp:
        raise cv.Invalid("Only one of 'pca6416a_id' or 'mcp23017_id' may be specified")
    return config


def _set_model_id_type(config):
    config[CONF_ID].type = _CPP_CLASSES[config[CONF_MODEL]]
    return config


def _inject_direct_pin_defaults(config):
    model = MODELS[config[CONF_MODEL]]
    for pin_name, gpio_num in model.direct_pins.items():
        conf_key = _DIRECT_PIN_CONF.get(pin_name)
        if conf_key and conf_key not in config:
            config[conf_key] = esphome_pins.gpio_output_pin_schema({"number": gpio_num})
    return config


def _final_validate(config):
    if CONF_LAMBDA in config or CONF_PAGES in config:
        if CONF_UPDATE_INTERVAL not in config:
            config[CONF_UPDATE_INTERVAL] = update_interval("1min")

    model = MODELS.get(config.get(CONF_MODEL))
    if model is not None and model.min_update_interval_ms > 0:
        interval = config.get(CONF_UPDATE_INTERVAL)
        if interval is not None and interval.total_milliseconds < model.min_update_interval_ms:
            raise cv.Invalid(
                f"update_interval {interval.total_milliseconds}ms below minimum "
                f"{model.min_update_interval_ms}ms for {model.name}"
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID():                    cv.declare_id(next(iter(_CPP_CLASSES.values()))),
            cv.Required(CONF_MODEL):            cv.one_of(*MODELS, lower=True),
            cv.Optional(CONF_PCA6416A_ID):      cv.use_id(PCA6416AComponent),
            cv.Optional(CONF_MCP23017_ID):      cv.use_id(MCP23017Component),
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=1): cv.positive_int,
            cv.Optional(CONF_GRAYSCALE_MODE, default=False): cv.boolean,
            # Direct GPIO pin overrides (defaults injected from model by _inject_direct_pin_defaults)
            cv.Optional(CONF_PIN_CKV):          esphome_pins.gpio_output_pin_schema,
            cv.Optional(CONF_PIN_SPH):          esphome_pins.gpio_output_pin_schema,
            cv.Optional(CONF_PIN_LE):           esphome_pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(display.FULL_DISPLAY_SCHEMA)
    .extend(i2c.i2c_device_schema(default_address=0x48)),
    _validate_expander,
    _set_model_id_type,
    _inject_direct_pin_defaults,
)


async def _make_expander_pin(pca_var, pin_num, pin_name, parent_id):
    pin_id = ID(f"{parent_id}_pca_{pin_name}", is_declaration=True, type=PCA6416AGPIOPin)
    pin_var = cg.new_Pvariable(pin_id)
    cg.add(pin_var.set_parent(pca_var))
    cg.add(pin_var.set_pin(pin_num))
    cg.add(pin_var.set_inverted(False))
    cg.add(pin_var.set_flags(cg.RawExpression("gpio::FLAG_OUTPUT")))
    return pin_var


async def _make_mcp23017_pin(mcp_var, pin_num, pin_name, parent_id):
    pin_id = ID(f"{parent_id}_mcp_{pin_name}", is_declaration=True, type=MCP23017GPIOPin)
    pin_var = cg.new_Pvariable(pin_id)
    cg.add(pin_var.set_parent(mcp_var))
    cg.add(pin_var.set_pin(pin_num))
    cg.add(pin_var.set_inverted(False))
    cg.add(pin_var.set_flags(cg.RawExpression("gpio::FLAG_OUTPUT")))
    return pin_var


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]
    var = cg.new_Pvariable(config[CONF_ID], model.width, model.height,
                           model.dark_phases, model.partial_phases, model.grayscale_phases)

    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    cg.add(var.set_grayscale_mode(config[CONF_GRAYSCALE_MODE]))
    if model.gpio0_enable_low:
        cg.add(var.set_gpio0_enable_low(True))

    # Direct GPIO pins
    direct_setters = {
        CONF_PIN_CKV: "set_pin_ckv",
        CONF_PIN_SPH: "set_pin_sph",
        CONF_PIN_LE:  "set_pin_le",
    }
    for conf_key, setter in direct_setters.items():
        if conf_key in config:
            pin = await cg.gpio_pin_expression(config[conf_key])
            cg.add(getattr(var, setter)(pin))

    # Expander pins — auto-wired from pca6416a_id or mcp23017_id
    expander_setters = {
        "oe":           "set_pin_oe",
        "gmod":         "set_pin_gmod",
        "spv":          "set_pin_spv",
        "wakeup":       "set_pin_wakeup",
        "pwrup":        "set_pin_pwrup",
        "vcom":         "set_pin_vcom",
        "gpio0_enable": "set_pin_gpio0_enable",
    }
    if CONF_PCA6416A_ID in config:
        exp_var = await cg.get_variable(config[CONF_PCA6416A_ID])
        make_pin = _make_expander_pin
    else:
        exp_var = await cg.get_variable(config[CONF_MCP23017_ID])
        make_pin = _make_mcp23017_pin
    for pin_name, setter in expander_setters.items():
        pin_num = model.expander_pins[pin_name]
        pin = await make_pin(exp_var, pin_num, pin_name, config[CONF_ID].id)
        cg.add(getattr(var, setter)(pin))

    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.display_ns.class_("Display").operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
