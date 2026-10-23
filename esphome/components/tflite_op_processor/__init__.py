"""Operator resolution for TFLite Micro models.

Library-only component: no user-facing entities. Loaded as a
dependency of tflite_micro_runtime; its C++ sources are compiled into
the firmware and used by the model handler to register the operators
required by a model.
"""

import logging

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_op_processor.op_manifest import (
    build_selective_op_defines,
)
import esphome.config_validation as cv
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_op_processor"

CONF_OPERATORS = "operators"

_SELECTIVE_FLAG_PREFIX = "TFLM_OP_ENABLED_"

# Library-only component: the schema stays small, but it accepts the operator
# list a component can supply so the selective build can also be exercised on
# its own (tflite_micro_runtime derives the same list from the models).
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OPERATORS): cv.All(
            cv.ensure_list(cv.string), cv.Length(min=1)
        ),
    }
)


async def to_code(config):
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # The TFLM_OP_CONDITIONAL operators in tflm_operators.h need 1.3.7 or newer.
    # Keep this pin in sync with micro_wake_word and esphome/idf_component.yml:
    # add_idf_component() keeps the last ref it is given, so a different pin
    # elsewhere in the config would make the version depend on load order.
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
    if (operators := config.get(CONF_OPERATORS)) is not None:
        _emit_selective_op_defines(operators)


def _emit_selective_op_defines(operators: list[str]) -> None:
    """Compile in only the kernels ``operators`` need, leaving the rest out.

    The flags are process-global, so they are emitted once: tflite_micro_runtime
    computes the same set for the whole build from the configured models, and a
    second set would define every enum value twice.
    """
    if any(define.name.startswith(_SELECTIVE_FLAG_PREFIX) for define in CORE.defines):
        _LOGGER.debug("Selective operator flags are already set, keeping them")
        return
    try:
        defines = build_selective_op_defines(operators)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    cg.add_define("USE_TFLITE_MICRO_SELECTIVE_OPS")
    for suffix, enabled in defines.items():
        cg.add_define(f"{_SELECTIVE_FLAG_PREFIX}{suffix}", enabled)
