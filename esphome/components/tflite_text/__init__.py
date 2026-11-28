"""Text pipeline for tflite_micro_runtime.

A text model takes token ids, an attention mask and a segment mask as int32 tensors,
and returns logits or start/end positions. With a ``tokenizer`` the pipeline takes
UTF-8 text and tokenizes it; without one the consumer passes the token ids.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    CONF_PIPELINE_ID,
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import read_model_io
from esphome.components.tflite_output_processor import CONF_DECODER_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_text"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

CONF_POSITION_OUTPUT = "position_output"
CONF_TOKENIZER = "tokenizer"
CONF_VOCABULARY = "vocabulary"
CONF_LOWERCASE = "lowercase"

# The output types read_tensor_value() reads.
OUTPUT_TYPES = ("float32", "uint8", "int8", "int32")
# The tokenizer stores ids in 16 bits.
MAX_VOCABULARY_SIZE = 65536
# Unknown, start, end and padding, in the order the tokenizer takes them.
SPECIAL_TOKENS = ("[UNK]", "[CLS]", "[SEP]", "[PAD]")

tflite_text_ns = cg.esphome_ns.namespace("tflite_text")
TextInputPipeline = tflite_text_ns.class_("TextInputPipeline")
TextDecoder = tflite_text_ns.class_("TextDecoder")
WordPieceTokenizer = tflite_text_ns.class_("WordPieceTokenizer")


def load_vocabulary(path: Path) -> list[bytes]:
    """The tokens of a ``vocab.txt``, one per line, in id order."""
    try:
        data = Path(path).read_bytes()
        data.decode("utf-8")
    except (OSError, UnicodeDecodeError) as err:
        raise cv.Invalid(f"Cannot read the vocabulary '{path}': {err}") from err
    lines = data.split(b"\n")
    if lines[-1] == b"":
        lines.pop()
    tokens = [line.removesuffix(b"\r") for line in lines]
    if not tokens or len(tokens) > MAX_VOCABULARY_SIZE:
        raise cv.Invalid(
            f"The vocabulary needs 1 to {MAX_VOCABULARY_SIZE} tokens, not {len(tokens)}"
        )
    seen: set[bytes] = set()
    for line, token in enumerate(tokens, start=1):
        if not token or b"\0" in token or token in seen:
            raise cv.Invalid(
                f"Line {line} of the vocabulary is empty, repeated or holds a NUL byte"
            )
        seen.add(token)
    if missing := [name for name in SPECIAL_TOKENS if name.encode() not in seen]:
        raise cv.Invalid(f"The vocabulary lacks {', '.join(missing)}")
    return tokens


def _validate_vocabulary(config: ConfigType) -> ConfigType:
    load_vocabulary(config[CONF_VOCABULARY])
    return config


TOKENIZER_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_VOCABULARY): cv.file_,
            cv.Optional(CONF_LOWERCASE, default=True): cv.boolean,
            cv.GenerateID(): cv.declare_id(WordPieceTokenizer),
        }
    ),
    _validate_vocabulary,
)


async def _new_tokenizer(config: ConfigType) -> cg.MockObj:
    """Emit the vocabulary, sorted by bytes for the binary search, and the tokenizer."""
    tokens = load_vocabulary(config[CONF_VOCABULARY])
    text = bytearray()
    offsets: list[int] = []
    ids: list[int] = []
    for index in sorted(range(len(tokens)), key=tokens.__getitem__):
        offsets.append(len(text))
        ids.append(index)
        text += tokens[index] + b"\0"
    return cg.new_Pvariable(
        config[CONF_ID],
        cg.shared_progmem_array("tflite_text_vocabulary", cg.uint8, list(text)),
        cg.shared_progmem_array("tflite_text_offsets", cg.uint32, offsets),
        cg.shared_progmem_array("tflite_text_ids", cg.uint16, ids),
        len(tokens),
        *(tokens.index(name.encode()) for name in SPECIAL_TOKENS),
        config[CONF_LOWERCASE],
    )


TEXT_SCHEMA = cv.Schema(
    {
        # A two-entry output is ambiguous: two class logits or start/end positions.
        cv.Optional(CONF_POSITION_OUTPUT, default=False): cv.boolean,
        cv.Optional(CONF_TOKENIZER): TOKENIZER_SCHEMA,
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(TextInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(TextDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(TEXT_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path, position_output: bool = False) -> None:
    """Reject a model whose tensors the text pipeline cannot fill."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if not 1 <= len(model_io.inputs) <= 3:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.inputs)} input tensors; a text "
            f"model has 1 to 3"
        )
    for index, spec in enumerate(model_io.inputs):
        if (
            spec.type != "int32"
            or len(spec.shape) != 2
            or spec.shape[0] != 1
            or spec.shape[1] <= 0
        ):
            raise cv.Invalid(
                f"Model '{name}' input {index} is {spec.type} {list(spec.shape)}; "
                f"a text model needs [1, length] int32 tensors"
            )
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"text model needs one"
        )
    output = model_io.outputs[0]
    if output.type not in OUTPUT_TYPES:
        raise cv.Invalid(
            f"Model '{name}' declares a {output.type} output; the decoder reads "
            f"{', '.join(OUTPUT_TYPES)}"
        )
    if len(output.shape) != 2 or output.shape[0] != 1 or output.shape[1] <= 0:
        raise cv.Invalid(
            f"Model '{name}' declares the output {list(output.shape)}; the "
            f"decoder needs [1, classes]"
        )
    if position_output and output.shape[1] != 2:
        raise cv.Invalid(
            f"position_output needs a [1, 2] output (start and end), but model "
            f"'{name}' declares {list(output.shape)}"
        )


async def configure(var: cg.MockObj, config: ConfigType, model_path: Path) -> None:
    """Create the token pipeline and the output decoder for one model."""
    validate_model_io(model_path, config[CONF_POSITION_OUTPUT])
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    if (tokenizer := config.get(CONF_TOKENIZER)) is not None:
        cg.add(pipeline.set_tokenizer(await _new_tokenizer(tokenizer)))
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID], config[CONF_POSITION_OUTPUT])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
