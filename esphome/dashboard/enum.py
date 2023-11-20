"""Enum backports from standard lib."""
from __future__ import annotations

from enum import Enum
from typing import Any


class StrEnum(str, Enum):
    """Partial backport of Python 3.11's StrEnum for our basic use cases."""

    def __new__(cls, value: str, *args: Any, **kwargs: Any) -> StrEnum:
        """Create a new StrEnum instance."""
        if not isinstance(value, str):
            raise TypeError(f"{value!r} is not a string")
        return super().__new__(cls, value, *args, **kwargs)

    def __str__(self) -> str:
        """Return self.value."""
        return str(self.value)
