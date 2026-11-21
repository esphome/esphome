"""Read a TFLite model's flatbuffer without TensorFlow; every read is bounds checked."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

from .builtin_codes import BUILTIN_OPERATOR_NAMES

_TFL3_IDENTIFIER = b"TFL3"

# Field ids of the Model, Buffer, OperatorCode, SubGraph, Operator and Tensor tables.
_MODEL_OPERATOR_CODES = 1
_MODEL_SUBGRAPHS = 2
_MODEL_BUFFERS = 4
_BUFFER_DATA = 0
_OPCODE_DEPRECATED_BUILTIN_CODE = 0
_OPCODE_BUILTIN_CODE = 3
_SUBGRAPH_TENSORS = 0
_SUBGRAPH_INPUTS = 1
_SUBGRAPH_OUTPUTS = 2
_SUBGRAPH_OPERATORS = 3
_OPERATOR_INPUTS = 1
_OPERATOR_OUTPUTS = 2
_TENSOR_SHAPE = 0
_TENSOR_TYPE = 1
_TENSOR_BUFFER = 2
_TENSOR_QUANTIZATION = 4

# Fields of the QuantizationParameters table, one entry per tensor.
_QUANT_SCALE = 2
_QUANT_ZERO_POINT = 3

# Bytes per element, following ``TensorTypeGetSize()``; an unsupported type is absent.
TENSOR_TYPE_SIZES: dict[int, int] = {
    0: 4,  # FLOAT32
    1: 2,  # FLOAT16
    2: 4,  # INT32
    3: 1,  # UINT8
    4: 8,  # INT64
    6: 1,  # BOOL
    7: 2,  # INT16
    8: 8,  # COMPLEX64
    9: 1,  # INT8
    10: 8,  # FLOAT64
    11: 16,  # COMPLEX128
    12: 8,  # UINT64
    15: 4,  # UINT32
    16: 2,  # UINT16
}

# The input tensor types the runtime can fill, as the YAML input_type names them.
INPUT_TENSOR_TYPES: dict[int, str] = {0: "float32", 3: "uint8", 9: "int8"}

# The TFLite TensorType names, for describing any input or output tensor.
TENSOR_TYPE_NAMES: dict[int, str] = {
    0: "float32",
    1: "float16",
    2: "int32",
    3: "uint8",
    4: "int64",
    5: "string",
    6: "bool",
    7: "int16",
    8: "complex64",
    9: "int8",
    10: "float64",
    11: "complex128",
    12: "uint64",
    13: "resource",
    14: "variant",
    15: "uint32",
    16: "uint16",
    17: "int4",
    18: "bfloat16",
}

# Calibrated against TFLM's arena_used_bytes() on quantized models: TFLM needed 1.8x to 2.1x the live peak.
_ARENA_SAFETY_FACTOR_TENTHS = 22
_ARENA_HEAD_OVERHEAD = 64
_ARENA_HEAD_PER_TENSOR = 12
_ARENA_ROUNDING = 1024


class ModelReadError(ValueError):
    """A ``.tflite`` model could not be read."""


@dataclass(frozen=True)
class ArenaEstimate:
    """A compile-time estimate of the tensor arena a model needs."""

    bytes: int  # rounded up to 1 KiB
    peak_activation_bytes: int  # held by the tensors live at the same time
    tensor_count: int  # tensors in the model's main subgraph


@dataclass(frozen=True)
class TensorSpec:
    """The shape, element type and quantization of one model tensor."""

    shape: tuple[int, ...]
    type: str  # the TFLite type name of the tensor
    scale: float = 1.0  # dequantization scale, 1.0 for a float tensor
    zero_point: int = 0


@dataclass(frozen=True)
class ModelIo:
    """Every input and output tensor a model declares."""

    inputs: tuple[TensorSpec, ...]
    outputs: tuple[TensorSpec, ...]


@dataclass(frozen=True)
class _Tensor:
    size: int  # bytes, 0 for a type with no known element size
    in_arena: bool  # allocated in the arena rather than read from the model


class _FlatBuffer:
    """Bounds-checked reads of the flatbuffer tables this module needs."""

    __slots__ = ("path", "_data")

    def __init__(self, data: bytes, path: Path) -> None:
        self.path = path
        self._data = data

    def _require(self, position: int, size: int) -> None:
        if position < 0 or position + size > len(self._data):
            raise ModelReadError(
                f"{self.path}: cannot read {size} byte(s) at offset {position} of a "
                f"{len(self._data)} byte file; the model is truncated or corrupt"
            )

    def _unpack(self, fmt: str, position: int) -> int:
        self._require(position, struct.calcsize(fmt))
        return struct.unpack_from(fmt, self._data, position)[0]

    def i8(self, position: int) -> int:
        return self._unpack("<b", position)

    def u16(self, position: int) -> int:
        return self._unpack("<H", position)

    def u32(self, position: int) -> int:
        return self._unpack("<I", position)

    def i32(self, position: int) -> int:
        return self._unpack("<i", position)

    def root(self) -> int:
        return self.u32(0)

    def field(self, table: int, field_id: int) -> int | None:
        """Position of a table field's value, or None when the field is absent."""
        # The vtable holds one 16-bit offset per field after the two sizes.
        vtable = table - self.i32(table)
        entry = 4 + 2 * field_id
        if entry >= self.u16(vtable):
            return None
        offset = self.u16(vtable + entry)
        return None if offset == 0 else table + offset

    def indirect(self, position: int) -> int:
        """Target of an offset, as stored for a table, vector or string field."""
        return position + self.u32(position)

    def _vector(self, table: int, field_id: int, element_size: int) -> tuple[int, int]:
        """Position of the first element and the element count, or (0, 0)."""
        field = self.field(table, field_id)
        if field is None:
            return 0, 0
        start = self.indirect(field)
        count = self.u32(start)
        elements = start + 4
        self._require(elements, count * element_size)
        return elements, count

    def vector_length(self, table: int, field_id: int, element_size: int) -> int:
        """Element count of a vector field, 0 when the field is absent."""
        return self._vector(table, field_id, element_size)[1]

    def i32_vector(self, table: int, field_id: int) -> list[int]:
        """Elements of an int vector field, empty when the field is absent."""
        elements, count = self._vector(table, field_id, 4)
        if not count:
            return []
        return list(struct.unpack_from(f"<{count}i", self._data, elements))

    def f32_vector(self, table: int, field_id: int) -> list[float]:
        """Elements of a float vector field, empty when absent."""
        elements, count = self._vector(table, field_id, 4)
        if not count:
            return []
        return list(struct.unpack_from(f"<{count}f", self._data, elements))

    def table_vector(self, table: int, field_id: int) -> list[int]:
        """Positions of the tables in a table vector field, empty when absent."""
        elements, count = self._vector(table, field_id, 4)
        return [self.indirect(elements + 4 * index) for index in range(count)]

    def i8_field(self, table: int, field_id: int) -> int | None:
        position = self.field(table, field_id)
        return None if position is None else self.i8(position)

    def i32_field(self, table: int, field_id: int) -> int | None:
        position = self.field(table, field_id)
        return None if position is None else self.i32(position)

    def u32_field(self, table: int, field_id: int) -> int | None:
        position = self.field(table, field_id)
        return None if position is None else self.u32(position)


def _load(model_path: str | Path) -> _FlatBuffer:
    """Read a model file, checking that it is a flatbuffer-wrapped .tflite."""
    path = Path(model_path)
    try:
        data = path.read_bytes()
    except OSError as err:
        raise ModelReadError(f"{path}: {err.strerror or err}") from err
    if data[4:8] != _TFL3_IDENTIFIER:
        raise ModelReadError(
            f"{path}: the file does not start with the TFL3 identifier of a TFLite "
            f"model; it is corrupt or is not a .tflite file"
        )
    return _FlatBuffer(data, path)


def _operator_codes(buffer: _FlatBuffer) -> list[int]:
    """The builtin code of every operator the model declares."""
    codes = []
    for table in buffer.table_vector(buffer.root(), _MODEL_OPERATOR_CODES):
        # GetBuiltinCode() takes the larger field: the older schema leaves builtin_code at 0, and a code past 127
        # does not fit the byte field.
        deprecated = buffer.i8_field(table, _OPCODE_DEPRECATED_BUILTIN_CODE) or 0
        builtin = buffer.i32_field(table, _OPCODE_BUILTIN_CODE) or 0
        codes.append(max(builtin, deprecated))
    if not codes:
        raise ModelReadError(f"{buffer.path}: the model declares no operator codes")
    return codes


def _operator_name(code: int) -> str:
    """Name of a builtin code, or its number when the schema is newer than this table."""
    if (name := BUILTIN_OPERATOR_NAMES.get(code)) is not None:
        return name
    return f"BUILTIN_{code}"


def read_model_operators(model_path: str | Path) -> frozenset[str]:
    """The operators a model declares, one entry per code, so ``len()`` is the resolver slots it needs."""
    return frozenset(
        _operator_name(code) for code in _operator_codes(_load(model_path))
    )


def _subgraph(buffer: _FlatBuffer) -> int:
    if not (subgraphs := buffer.table_vector(buffer.root(), _MODEL_SUBGRAPHS)):
        raise ModelReadError(f"{buffer.path}: the model has no subgraph")
    return subgraphs[0]


def _tensor_spec(buffer: _FlatBuffer, table: int) -> TensorSpec:
    """The shape, type and per-tensor quantization of one tensor table."""
    code = buffer.i8_field(table, _TENSOR_TYPE) or 0
    if (type_name := TENSOR_TYPE_NAMES.get(code)) is None:
        raise ModelReadError(
            f"{buffer.path}: tensor type {code} is not one of the known types"
        )
    scale, zero_point = 1.0, 0
    if (position := buffer.field(table, _TENSOR_QUANTIZATION)) is not None:
        quantization = buffer.indirect(position)
        if scales := buffer.f32_vector(quantization, _QUANT_SCALE):
            scale = scales[0]
        if zero_points := buffer.i32_vector(quantization, _QUANT_ZERO_POINT):
            zero_point = zero_points[0]
    return TensorSpec(
        shape=tuple(buffer.i32_vector(table, _TENSOR_SHAPE)),
        type=type_name,
        scale=scale,
        zero_point=zero_point,
    )


def _tensor_specs(
    buffer: _FlatBuffer, subgraph: int, field_id: int
) -> tuple[TensorSpec, ...]:
    """The tensors a subgraph names in one index vector, in order."""
    tensors = buffer.table_vector(subgraph, _SUBGRAPH_TENSORS)
    specs = []
    for index in buffer.i32_vector(subgraph, field_id):
        if not 0 <= index < len(tensors):
            raise ModelReadError(
                f"{buffer.path}: the subgraph names tensor {index}, but it has {len(tensors)}"
            )
        specs.append(_tensor_spec(buffer, tensors[index]))
    return tuple(specs)


def read_model_io(model_path: str | Path) -> ModelIo:
    """Every input and output tensor a model declares, with its shape as stored."""
    buffer = _load(model_path)
    subgraph = _subgraph(buffer)
    return ModelIo(
        inputs=_tensor_specs(buffer, subgraph, _SUBGRAPH_INPUTS),
        outputs=_tensor_specs(buffer, subgraph, _SUBGRAPH_OUTPUTS),
    )


def read_model_input(model_path: str | Path) -> TensorSpec:
    """The first input tensor a model declares, with its shape as stored."""
    model_io = read_model_io(model_path)
    if not model_io.inputs:
        raise ModelReadError(f"{Path(model_path)}: the model declares no input tensor")
    first = model_io.inputs[0]
    if first.type not in INPUT_TENSOR_TYPES.values():
        raise ModelReadError(
            f"{Path(model_path)}: input tensor type '{first.type}' is not one the runtime can fill"
        )
    return first


def _buffers_with_data(buffer: _FlatBuffer) -> list[bool]:
    """Whether each model buffer holds the constant bytes of a tensor."""
    return [
        buffer.vector_length(table, _BUFFER_DATA, 1) > 0
        for table in buffer.table_vector(buffer.root(), _MODEL_BUFFERS)
    ]


def _read_tensors(
    buffer: _FlatBuffer, subgraph: int, constant_buffers: list[bool]
) -> list[_Tensor]:
    """Every tensor of a subgraph, with the bytes it occupies in the arena."""
    tensors = []
    for table in buffer.table_vector(subgraph, _SUBGRAPH_TENSORS):
        type_code = buffer.i8_field(table, _TENSOR_TYPE) or 0
        if (element_size := TENSOR_TYPE_SIZES.get(type_code)) is None:
            raise ModelReadError(
                f"{buffer.path}: tensor type {type_code} has no known element size, "
                f"so the arena it needs cannot be estimated"
            )
        elements = 1
        for dimension in buffer.i32_vector(table, _TENSOR_SHAPE):
            if dimension < 0:
                raise ModelReadError(
                    f"{buffer.path}: tensor dimension {dimension} is negative"
                )
            elements *= dimension
        # A tensor whose buffer holds data is a constant read in place from the model; the converter gives the
        # others a buffer entry too, usually empty.
        index = buffer.u32_field(table, _TENSOR_BUFFER) or 0
        in_arena = not (index < len(constant_buffers) and constant_buffers[index])
        tensors.append(_Tensor(element_size * elements, in_arena))
    return tensors


def _read_operators(
    buffer: _FlatBuffer, subgraph: int
) -> list[tuple[list[int], list[int]]]:
    """The input and output tensor indexes of every operator, in execution order."""
    return [
        (
            # -1 is TFLite's "optional tensor omitted", such as a fully connected bias.
            [
                index
                for index in buffer.i32_vector(table, _OPERATOR_INPUTS)
                if index != -1
            ],
            buffer.i32_vector(table, _OPERATOR_OUTPUTS),
        )
        for table in buffer.table_vector(subgraph, _SUBGRAPH_OPERATORS)
    ]


def _peak_activation_bytes(
    tensors: list[_Tensor],
    operators: list[tuple[list[int], list[int]]],
    inputs: list[int],
    outputs: list[int],
) -> int:
    """Largest total size of the tensors live at the same point.

    Subgraph inputs are live from the start and subgraph outputs until the end, so a
    graph whose branches only meet at the output does not understate the peak.
    """
    first_use: dict[int, int] = {}
    last_use: dict[int, int] = {}
    last_operator = len(operators) - 1
    for tensor in inputs:
        if not 0 <= tensor < len(tensors):
            raise ModelReadError(
                f"the subgraph input {tensor} is outside the {len(tensors)} tensors"
            )
        first_use[tensor] = 0
        last_use.setdefault(tensor, 0)
    for tensor in outputs:
        if not 0 <= tensor < len(tensors):
            raise ModelReadError(
                f"the subgraph output {tensor} is outside the {len(tensors)} tensors"
            )
        last_use[tensor] = last_operator
    for index, (op_inputs, op_outputs) in enumerate(operators):
        for tensor in (*op_inputs, *op_outputs):
            if not 0 <= tensor < len(tensors):
                raise ModelReadError(
                    f"operator {index} refers to tensor {tensor}, but the subgraph has "
                    f"{len(tensors)}"
                )
            first_use.setdefault(tensor, index)
            last_use[tensor] = max(index, last_use.get(tensor, index))
    arena_tensors = [
        (index, tensor.size)
        for index, tensor in enumerate(tensors)
        if tensor.in_arena and index in first_use
    ]
    peak = 0
    for index in range(len(operators)):
        live = sum(
            size
            for tensor, size in arena_tensors
            if first_use[tensor] <= index <= last_use[tensor]
        )
        peak = max(peak, live)
    return peak


def estimate_tensor_arena(model_path: str | Path) -> ArenaEstimate:
    """Estimate the arena size from the model; the device probe measures the exact size."""
    buffer = _load(model_path)
    subgraph = _subgraph(buffer)
    tensors = _read_tensors(buffer, subgraph, _buffers_with_data(buffer))
    peak = _peak_activation_bytes(
        tensors,
        _read_operators(buffer, subgraph),
        buffer.i32_vector(subgraph, _SUBGRAPH_INPUTS),
        buffer.i32_vector(subgraph, _SUBGRAPH_OUTPUTS),
    )
    head = (_ARENA_HEAD_OVERHEAD + _ARENA_HEAD_PER_TENSOR * len(tensors) + 63) & ~63
    raw = head + peak * _ARENA_SAFETY_FACTOR_TENTHS // 10
    return ArenaEstimate(
        bytes=((raw + _ARENA_ROUNDING - 1) // _ARENA_ROUNDING) * _ARENA_ROUNDING,
        peak_activation_bytes=peak,
        tensor_count=len(tensors),
    )
