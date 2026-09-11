"""Build-time scans of the user's C++ sources: lambdas, ``includes:`` files and
external components.

The only scan today finds scanf calls with float conversions, which decides
whether float scanf support (or, on ESP-IDF Bluetooth builds, the libc sscanf)
must stay linked. Other source heuristics belong here too.
"""

import logging
from pathlib import Path
import re

from esphome.config_helpers import iter_include_files
from esphome.const import (
    CONF_ESPHOME,
    CONF_INCLUDES,
    CONF_INCLUDES_C,
    SOURCE_FILE_EXTENSIONS,
)
from esphome.core import CORE, Lambda
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# The string literal handed to a scanf family call as its format: an optional
# first argument (sscanf/fscanf; a literal or a call with its own commas is
# fine) then the literal.
_SCANF_FORMAT_RE = re.compile(
    r'scanf\s*\((?:(?:"(?:[^"\\]|\\.)*"|\([^()]*\)|[^,;"()])*,)?\s*"((?:[^"\\]|\\.)*)"'
)
# Standard scanf float conversions %f %F %e %E %g %G %a %A with optional
# suppression, width and length; also the invalid %.2f users write by analogy
# with printf.
_SCANF_FLOAT_SPEC_RE = re.compile(r"%[*\d.]*[hlL]*[feEgGaAF]")


def source_uses_scanf_float(src: str) -> bool:
    """Heuristic: does C++ source call a scanf family function with a float conversion?"""
    if "scanf" not in src:
        return False
    src = Lambda.comment_remover(src)
    return any(
        _SCANF_FLOAT_SPEC_RE.search(match.group(1))
        for match in _SCANF_FORMAT_RE.finditer(src)
    )


def lambdas_use_scanf_float(config: ConfigType) -> bool:
    """Check if any lambda in the config uses scanf with a float format specifier."""
    stack: list = [config]
    while stack:
        obj = stack.pop()
        if isinstance(obj, Lambda):
            if source_uses_scanf_float(obj.value):
                return True
        elif isinstance(obj, dict):
            stack.extend(obj.values())
        elif isinstance(obj, list):
            stack.extend(obj)
    return False


def includes_use_scanf_float(config: ConfigType) -> bool:
    """Check if an ``includes:`` or ``includes_c:`` file uses scanf with a float format specifier."""
    esphome_config = config.get(CONF_ESPHOME, {})
    includes = esphome_config.get(CONF_INCLUDES, []) + esphome_config.get(
        CONF_INCLUDES_C, []
    )
    for path, _ in iter_include_files(includes):
        if path.suffix not in SOURCE_FILE_EXTENSIONS:
            continue
        if source_uses_scanf_float(path.read_text(encoding="utf-8", errors="replace")):
            return True
    return False


def external_components_use_scanf_float() -> bool:
    """Whether a component from outside the esphome package scans a float."""
    from esphome.config import iter_components
    from esphome.loader import CORE_COMPONENTS_PATH

    package_root = CORE_COMPONENTS_PATH.parent
    for _, component in iter_components(CORE.config):
        package_dir = Path(component.module.__file__).resolve().parent
        if package_dir.is_relative_to(package_root):
            continue
        for resource in component.resources:
            src = (package_dir / resource.resource).read_text(
                encoding="utf-8", errors="replace"
            )
            if source_uses_scanf_float(src):
                return True
    return False


def user_code_uses_scanf_float(config: ConfigType) -> bool:
    """Whether a lambda, an ``includes:`` file or an external component scans a float."""
    return (
        lambdas_use_scanf_float(config)
        or includes_use_scanf_float(config)
        or external_components_use_scanf_float()
    )


def keep_float_scanf(
    option: bool | None, config: ConfigType, flash_note: str, override_note: str
) -> bool:
    """Resolve a tri-state float-scanf option: unset means "only if user code scans a float"."""
    if option is None:
        if not user_code_uses_scanf_float(config):
            return False
        _LOGGER.warning(
            "Lambda, include or external component uses scanf with a float format "
            "specifier; keeping float scanf support (%s)",
            flash_note,
        )
        return True
    if not option and user_code_uses_scanf_float(config):
        _LOGGER.warning(
            "Float scanf support is disabled but a lambda, include or external "
            "component uses scanf with a float format specifier; %s",
            override_note,
        )
    return option
