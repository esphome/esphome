"""CTC decoding for tflite_micro_runtime, for speech recognition and similar models.

The model's single output is a [1, steps, classes] score matrix, decoded greedily:
the best class of each step, repeats collapsed and blanks dropped. The consumer
computes the input features and fills the input tensor.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import read_model_io
from esphome.components.tflite_model_source import resolve_model_source
from esphome.components.tflite_output_processor import CONF_DECODER_ID
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_ctc"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True

CONF_BLANK_INDEX = "blank_index"
CONF_LABELS = "labels"

# The decoder stores class ids in 16 bits.
MAX_CLASSES = 65536

tflite_ctc_ns = cg.esphome_ns.namespace("tflite_ctc")
CtcDecoder = tflite_ctc_ns.class_("CtcDecoder")

CTC_SCHEMA = cv.Schema(
    {
        # Defaults to the last class, as TensorFlow's CTC loss uses.
        cv.Optional(CONF_BLANK_INDEX): cv.int_range(min=0, max=MAX_CLASSES - 1),
        cv.Optional(CONF_LABELS): cv.All(
            cv.ensure_list(cv.string_strict), cv.Length(min=1)
        ),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(CtcDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(CTC_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path, config: ConfigType) -> tuple[int, int]:
    """Check the output matrix against the options; return its steps and the blank class."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a CTC "
            f"model needs one"
        )
    output = model_io.outputs[0]
    shape = list(output.shape)
    if (
        output.type not in ("float32", "int8", "uint8")
        or len(shape) != 3
        or shape[0] != 1
        or shape[1] <= 0
        or not 2 <= shape[2] <= MAX_CLASSES
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the output {output.type} {shape}; the decoder "
            f"needs [1, steps, classes] of type float32, int8 or uint8, with at least "
            f"2 classes"
        )
    classes = shape[2]
    blank = config.get(CONF_BLANK_INDEX, classes - 1)
    if blank >= classes:
        raise cv.Invalid(
            f"{CONF_BLANK_INDEX} is {blank}, but the model has {classes} classes"
        )
    if (labels := config.get(CONF_LABELS)) is not None and len(labels) != classes - 1:
        raise cv.Invalid(
            f"{CONF_LABELS} needs one entry for each of the {classes - 1} classes "
            f"other than the blank, not {len(labels)}"
        )
    return shape[1], blank


def _final_validate(config: ConfigType) -> None:
    """Check the model's tensors and operators during config validation."""
    final_validate(config)
    model_path, _ = resolve_model_source(config)
    validate_model_io(model_path, config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the decoder."""
    var, model_path = await declare_model(config)
    steps, blank = validate_model_io(model_path, config)
    # Sized now, so decoding never allocates.
    ids = cg.static_buffer(config[CONF_DECODER_ID], cg.uint16, steps)
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID], blank, ids, steps)
    if (labels := config.get(CONF_LABELS)) is not None:
        array = cg.shared_progmem_array("tflite_ctc_labels", cg.const_char_ptr, labels)
        cg.add(decoder.set_labels(array))
    cg.add(var.set_output_decoder(decoder))
