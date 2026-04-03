"""Enum backports from standard lib."""

from __future__ import annotations

try:
    from enum import StrEnum as _StrEnum
except ImportError:
    from enum import Enum

    class _StrEnum(str, Enum):
        pass

StrEnum = _StrEnum
