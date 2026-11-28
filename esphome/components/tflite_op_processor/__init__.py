"""Register the operators a TFLite Micro model needs; loaded by tflite_micro_runtime."""

from dataclasses import dataclass
import logging
from pathlib import Path

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from .op_manifest import build_selective_op_defines, extract_model_ops, parse_op_table

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_model_io"]
DOMAIN = "tflite_op_processor"

CONF_OPERATORS = "operators"

# The build-wide resolver size, in operator slots.
MAX_OPERATORS_DEFINE = "TFLITE_MICRO_MAX_OPERATORS"

_SELECTIVE_DEFINE = "USE_TFLITE_MICRO_SELECTIVE_OPS"
_SELECTIVE_FLAG_PREFIX = "TFLM_OP_ENABLED_"


@dataclass
class _OpSelection:
    all_ops: bool = False
    defines_emitted: bool = False


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


# Library-only component: the schema only accepts the operator list a component can supply, so the selective build
# can be exercised without tflite_micro_runtime.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OPERATORS): cv.All(
            cv.ensure_list(cv.string), cv.Length(min=1), _validate_operators
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    # Emit the flag this component's sources need, so it also works standalone.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # Verified against 1.4.1, which provides the TFLM_OP_CONDITIONAL Add*() methods. Keep in sync with
    # micro_wake_word and the other tflite_* components: the last ref wins, and an unpinned esp-nn makes arena
    # sizes vary per build.
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")
    if (operators := config.get(CONF_OPERATORS)) is not None:
        # A pinned list can exceed what the YAML models need, and a replacement
        # model loads against this same resolver, so size it for the list too.
        _emit_max_operators(len(set(operators)))
        emit_selective_op_defines(operators)


def emit_selective_op_defines(operators: list[str]) -> None:
    """Compile in only the kernels ``operators`` need, leaving the rest out.
    The public entry point for every component that configures the selective build, because the flags are
    process-global. A kernel another component enabled stays enabled, so the set only widens; a component that does
    not know its operators calls :func:`emit_all_ops` instead, which cancels the flags.
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
    For a component that cannot tell which operators its model needs. The request is sticky, so the result does
    not depend on the order components run in.
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


def _model_filename(model_path: Path) -> str:
    """File name of the resolved model, with Windows separators normalised."""
    return Path(str(model_path).replace("\\", "/")).name


def _emit_max_operators(value: int) -> None:
    """Queue the resolver size, widening a value another component already set."""
    for define in list(CORE.defines):
        if define.name != MAX_OPERATORS_DEFINE:
            continue
        try:
            existing = int(str(define.value))
        except (TypeError, ValueError) as err:
            raise EsphomeError(
                f"{MAX_OPERATORS_DEFINE} was already set to {define.value!r}, "
                f"which is not a number; this build needs {value}"
            ) from err
        if existing >= value:
            return
        CORE.defines.discard(define)
        _LOGGER.info(
            "Raising %s from %d to %d for the configured models",
            MAX_OPERATORS_DEFINE,
            existing,
            value,
        )
    cg.add_define(MAX_OPERATORS_DEFINE, value)


def add_model_operator_defines(model_paths: list[Path]) -> None:
    """Size the resolver and enable the kernels every configured model needs.

    Called with the models a runtime gathers; one slot per distinct operator
    code, so the widest model sizes the resolver. The defines are build-wide,
    so this runs once however many models are configured.
    """
    data = _selection()
    if data.defines_emitted:
        return
    data.defines_emitted = True
    if not model_paths:
        # Without a configured model neither the size nor the kernels can be narrowed.
        emit_all_ops()
        return
    model_ops = [extract_model_ops(path) for path in model_paths]
    _emit_max_operators(max(len(ops) for ops in model_ops))
    emit_selective_op_defines(sorted(set().union(*model_ops)))


def validate_model_operators(model_path: Path) -> None:
    """Reject a model whose operators the pinned TFLM cannot register.

    The model file always lists its operator codes, so an unreadable one is a
    configuration error rather than a load failure on the device.
    """
    try:
        model_ops = extract_model_ops(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    supported_ops = parse_op_table().supported
    if unsupported_ops := sorted(model_ops - supported_ops):
        raise cv.Invalid(
            f"Model '{_model_filename(model_path)}' requires operator(s) "
            f"{', '.join(unsupported_ops)} which esp-tflite-micro 1.4.1 "
            f"does not support.\n"
            f"  Supported operators: {', '.join(sorted(supported_ops))}\n"
            f"  Re-export the model without the unsupported operator(s) "
            f"or choose a compatible model."
        )
