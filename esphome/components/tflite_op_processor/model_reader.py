"""Read a TFLite model's flatbuffer without TensorFlow.

The operator list and the tensor shapes a model declares live in the ``.tflite``
file, so reading them needs nothing beyond the standard library and cannot go stale
like a report. Every read is bounds checked, so a truncated model raises
:class:`ModelReadError` with the offset instead of reading past the end.
"""

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
_SUBGRAPH_OPERATORS = 3
_OPERATOR_INPUTS = 1
_OPERATOR_OUTPUTS = 2
_TENSOR_SHAPE = 0
_TENSOR_TYPE = 1
_TENSOR_BUFFER = 2

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

# Calibrated against TFLM's arena_used_bytes() on quantized models, where the size
# TFLM needed was 1.8x to 2.1x the peak of the tensors that are live together.
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

    def u8(self, position: int) -> int:
        return self._unpack("<B", position)

    def i8(self, position: int) -> int:
        return self._unpack("<b", position)

    def u16(self, position: int) -> int:
        return self._unpack("<H", position)

    def u32(self, position: int) -> int:
        return self._unpack("<I", position)

    def i32(self, position: int) -> int:
        return self._unpack("<i", position)

    def root(self) -> int:
        """Position of the root table."""
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

    def i32_vector(self, table: int, field_id: int) -> list[int]:
        """Elements of an int vector field, empty when the field is absent."""
        elements, count = self._vector(table, field_id, 4)
        if not count:
            return []
        return list(struct.unpack_from(f"<{count}i", self._data, elements))

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
        # GetBuiltinCode() takes the larger of the two fields: the older schema
        # leaves builtin_code at 0, and a code past 127 does not fit the byte field.
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
    """The operators a model declares, as ``tflm_operators.h`` names them.
    There is one entry per distinct operator code, so ``len()`` is also the number
    of resolver slots the model needs. Raises :class:`ModelReadError` when the file
    cannot be read or is not a valid model.
    """
    return frozenset(
        _operator_name(code) for code in _operator_codes(_load(model_path))
    )


def _subgraph(buffer: _FlatBuffer) -> int:
    """Position of the first (main) subgraph."""
    if not (subgraphs := buffer.table_vector(buffer.root(), _MODEL_SUBGRAPHS)):
        raise ModelReadError(f"{buffer.path}: the model has no subgraph")
    return subgraphs[0]


def _buffers_with_data(buffer: _FlatBuffer) -> list[bool]:
    """Whether each entry of the model's buffer table holds constant data."""
    return [
        buffer._vector(table, _BUFFER_DATA, 1)[1] > 0
        for table in buffer.table_vector(buffer.root(), _MODEL_BUFFERS)
    ]


def _read_tensors(
    buffer: _FlatBuffer, subgraph: int, constant_buffers: list[bool]
) -> list[_Tensor]:
    """Every tensor of a subgraph, with the bytes it occupies in the arena."""
    tensors = []
    for table in buffer.table_vector(subgraph, _SUBGRAPH_TENSORS):
        element_size = TENSOR_TYPE_SIZES.get(
            buffer.i8_field(table, _TENSOR_TYPE) or 0, 0
        )
        elements = 1
        for dimension in buffer.i32_vector(table, _TENSOR_SHAPE):
            elements *= max(dimension, 0)
        # A tensor whose buffer holds data is a constant read in place from the
        # model; the converter gives the others a buffer entry too, usually empty.
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
            buffer.i32_vector(table, _OPERATOR_INPUTS),
            buffer.i32_vector(table, _OPERATOR_OUTPUTS),
        )
        for table in buffer.table_vector(subgraph, _SUBGRAPH_OPERATORS)
    ]


def _peak_activation_bytes(
    tensors: list[_Tensor], operators: list[tuple[list[int], list[int]]]
) -> int:
    """Largest total size of the tensors that are live at the same point.
    A tensor is live from the first operator that mentions it to the last.
    """
    first_use: dict[int, int] = {}
    last_use: dict[int, int] = {}
    for index, (inputs, outputs) in enumerate(operators):
        for tensor in (*inputs, *outputs):
            if not 0 <= tensor < len(tensors):
                continue
            first_use.setdefault(tensor, index)
            last_use[tensor] = index
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
    """Estimate the tensor arena a model needs, from the model itself.
    The device measures the exact size, so this is only the seed the probe starts
    from; a close one keeps its first attempt small.
    """
    buffer = _load(model_path)
    subgraph = _subgraph(buffer)
    tensors = _read_tensors(buffer, subgraph, _buffers_with_data(buffer))
    peak = _peak_activation_bytes(tensors, _read_operators(buffer, subgraph))
    head = (_ARENA_HEAD_OVERHEAD + _ARENA_HEAD_PER_TENSOR * len(tensors) + 63) & ~63
    raw = head + peak * _ARENA_SAFETY_FACTOR_TENTHS // 10
    return ArenaEstimate(
        bytes=((raw + _ARENA_ROUNDING - 1) // _ARENA_ROUNDING) * _ARENA_ROUNDING,
        peak_activation_bytes=peak,
        tensor_count=len(tensors),
    )
