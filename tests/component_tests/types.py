"""Type definitions for component tests."""

from __future__ import annotations

from typing import Protocol

from esphome.const import PlatformFramework
from esphome.types import ConfigType


class SetCoreConfigCallable(Protocol):
    """Protocol for the set_core_config fixture setter function."""

    def __call__(  # noqa: E704
        self,
        platform_framework: PlatformFramework,
        /,
        *,
        core_data: ConfigType | None = None,
        platform_data: ConfigType | None = None,
        full_config: dict[str, ConfigType] | None = None,
    ) -> None: ...
