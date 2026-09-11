"""Build-time scans of the user's C++ sources: lambdas, ``includes:`` files and
external components.

The only scan today finds scanf calls with float conversions, which decides
whether float scanf support (or, on ESP-IDF Bluetooth builds, the libc sscanf)
must stay linked. Other source heuristics belong here too.
"""

from collections.abc import Iterator
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

_SCANF_CALL_RE = re.compile(r"scanf\s*\(")
# Standard scanf float conversions %f %F %e %E %g %G %a %A with optional
# suppression, width and length; also the invalid %.2f users write by analogy
# with printf.
_SCANF_FLOAT_SPEC_RE = re.compile(r"%[*\d.]*[hlL]*[feEgGaAF]")


def _scanf_calls(src: str) -> Iterator[tuple[int, str]]:
    """Yield ``(format argument index, argument text)`` for each scanf family call.

    Walks to the closing parenthesis, treating string and character literals
    as opaque so a ';' inside a format string does not end the call early.
    """
    for match in _SCANF_CALL_RE.finditer(src):
        # sscanf and fscanf take the format second, plain scanf first
        fmt_index = 1 if match.start() > 0 and src[match.start() - 1] in "sf" else 0
        start = i = match.end()
        depth = 1
        quote = None
        while i < len(src):
            ch = src[i]
            if quote:
                if ch == "\\":
                    i += 1
                elif ch == quote:
                    quote = None
            elif ch in "\"'":
                quote = ch
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            elif ch == ";":
                break
            i += 1
        yield fmt_index, src[start:i]


def _argument(args: str, index: int) -> str | None:
    """The ``index``-th top-level argument of a call, or None if there are fewer."""
    depth = 0
    quote = None
    current = 0
    arg_start = 0
    for i, ch in enumerate(args):
        if quote:
            if ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            if current == index:
                return args[arg_start:i]
            current += 1
            arg_start = i + 1
    return args[arg_start:] if current == index else None


def source_uses_scanf_float(src: str) -> bool:
    """Heuristic: does C++ source call a scanf family function with a float conversion?

    A format that is not a string literal cannot be checked and counts as one.
    """
    if "scanf" not in src:
        return False
    src = Lambda.comment_remover(src)
    for fmt_index, args in _scanf_calls(src):
        fmt = _argument(args, fmt_index)
        if fmt is None or not fmt.lstrip().startswith('"'):
            return True
        # Join adjacent literal pieces such as "%" "f"
        if _SCANF_FLOAT_SPEC_RE.search(re.sub(r'"\s*"', "", fmt)):
            return True
    return False


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
