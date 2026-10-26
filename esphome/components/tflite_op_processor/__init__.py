"""Operator resolution for TFLite Micro models.

Library-only component: no user-facing entities. Loaded as a
dependency of tflite_micro_runtime; its C++ sources are compiled into
the firmware and used by the model handler to register the operators
required by a model.
"""

import logging

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.core import CORE

from .op_manifest import build_selective_op_defines

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
    # Keep these pins in sync with micro_wake_word, esphome/idf_component.yml and
    # the other tflite_* components: add_idf_component() keeps the last ref it is
    # given, so a different pin elsewhere would make the version depend on load
    # order. esp-nn is pinned with it because it decides how much memory each
    # kernel needs, so an unpinned esp-nn makes the arena sizes vary per build.
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.4.1")
    if (operators := config.get(CONF_OPERATORS)) is not None:
        emit_selective_op_defines(operators)


def emit_selective_op_defines(operators: list[str]) -> None:
    """Compile in only the kernels ``operators`` need, leaving the rest out.

    This is the public entry point for every component that configures the
    selective build, so all of them widen the same process-global flags through
    one place.

    The flags are process-global, so tflite_micro_runtime computes the same set
    for the whole build from the configured models. ``operators`` is always
    validated against the table, and a kernel this list needs is enabled even
    when another component already emitted the flags, so nothing is dropped
    without a trace: a component can widen the set, never narrow it.
    """
    try:
        defines = build_selective_op_defines(operators)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    cg.add_define("USE_TFLITE_MICRO_SELECTIVE_OPS")
    existing = {
        define.name: define
        for define in CORE.defines
        if define.name.startswith(_SELECTIVE_FLAG_PREFIX)
    }
    written = 0
    widened: list[str] = []
    for suffix, enabled in defines.items():
        name = f"{_SELECTIVE_FLAG_PREFIX}{suffix}"
        if (define := existing.get(name)) is not None:
            if str(define.value) == "1":
                # Another component needs this kernel: the set only widens.
                continue
            if not enabled:
                continue
            # Define hashes on its value, so replace it instead of mutating it.
            CORE.defines.discard(define)
            widened.append(suffix)
        cg.add_define(name, enabled)
        written += 1
    if widened:
        _LOGGER.info(
            "Enabled selective operator flags an earlier component left out: %s",
            ", ".join(widened),
        )
    else:
        _LOGGER.debug("Wrote %d selective operator flag(s)", written)
