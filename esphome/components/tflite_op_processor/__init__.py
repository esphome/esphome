"""Operator resolution for TFLite Micro models.

Library-only component, loaded by tflite_micro_runtime: its C++ sources register
the operators a model needs.
"""

from dataclasses import dataclass
import logging

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError

from .op_manifest import build_selective_op_defines

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_op_processor"

CONF_OPERATORS = "operators"

_SELECTIVE_DEFINE = "USE_TFLITE_MICRO_SELECTIVE_OPS"
_SELECTIVE_FLAG_PREFIX = "TFLM_OP_ENABLED_"


@dataclass
class _OpSelection:
    """Operator selection state of this configuration run."""

    all_ops: bool = False


def _selection() -> _OpSelection:
    if (data := CORE.data.get(DOMAIN)) is None:
        data = CORE.data[DOMAIN] = _OpSelection()
    return data


def _validate_operators(operators: list[str]) -> list[str]:
    """Reject a name the pinned TFLM cannot register while validating the YAML."""
    try:
        build_selective_op_defines(operators)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    return operators


# Library-only component: the schema only accepts the operator list a component
# can supply, so the selective build can be exercised without tflite_micro_runtime.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OPERATORS): cv.All(
            cv.ensure_list(cv.string), cv.Length(min=1), _validate_operators
        ),
    }
)


async def to_code(config):
    # Emit the flag this component's sources need, so it also works standalone.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # Verified against 1.4.1, which provides the TFLM_OP_CONDITIONAL Add*() methods.
    # Keep in sync with micro_wake_word and the other tflite_* components: the last
    # ref wins, and an unpinned esp-nn makes arena sizes vary per build.
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")
    if (operators := config.get(CONF_OPERATORS)) is not None:
        emit_selective_op_defines(operators)


def emit_selective_op_defines(operators: list[str]) -> None:
    """Compile in only the kernels ``operators`` need, leaving the rest out.
    The public entry point for every component that configures the selective build,
    because the flags are process-global. A kernel another component enabled stays
    enabled, so the set only widens; a component that does not know its operators
    calls :func:`emit_all_ops` instead, which cancels the flags.
    """
    try:
        defines = build_selective_op_defines(operators)
    except ValueError as err:
        # Only an EsphomeError is shown as a clean message during code generation.
        raise EsphomeError(str(err)) from err
    if _selection().all_ops:
        _LOGGER.debug(
            "Not limiting the kernels: every operator was requested for this build"
        )
        return
    cg.add_define(_SELECTIVE_DEFINE)
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


def emit_all_ops() -> None:
    """Compile in every kernel, cancelling any selective flags emitted so far.
    For a component that cannot tell which operators its model needs. The request
    is sticky, so the result does not depend on the order components run in.
    """
    _selection().all_ops = True
    dropped = 0
    for define in list(CORE.defines):
        if define.name == _SELECTIVE_DEFINE or define.name.startswith(
            _SELECTIVE_FLAG_PREFIX
        ):
            CORE.defines.discard(define)
            dropped += 1
    _LOGGER.info(
        "Compiling in every TFLite Micro kernel (%d selective flag(s) dropped)",
        dropped,
    )
