"""Enum backports from standard lib."""

from __future__ import annotations

from enum import StrEnum as _StrEnum

# Re-export StrEnum from standard library for backwards compatibility
StrEnum = _StrEnum
