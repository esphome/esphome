"""Host harness for ESP32 speaker-source handlers without FreeRTOS dependencies.

The callers' stubs mirror only the members used by the selected handlers. This
checks handler behavior, not the complete class ABI or FreeRTOS integration.
"""

from pathlib import Path
import re
import shutil
import subprocess

import pytest

# Consume literals and comments before considering braces. Raw strings may
# contain quotes and braces and must be matched as a single token.
_TOKENS = re.compile(
    r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"'
    r"|'(?:\\.|[^'\\])*'"
    r"|//[^\n]*|/\*.*?\*/|[{}]",
    re.DOTALL,
)


def method(source: str, qualified_name: str) -> str:
    """Extract an ordinary out-of-line handler, preserving its production body."""
    declaration = re.search(rf"(?m)^\w+\s+{re.escape(qualified_name)}\s*\(", source)
    assert declaration is not None, f"Missing C++ handler: {qualified_name}"
    depth = 0
    for token in _TOKENS.finditer(source, declaration.end()):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[declaration.start() : token.end()]
    raise AssertionError(f"Unterminated C++ handler: {qualified_name}")


def compile_and_run(cpp: Path, binary: Path) -> None:
    """Skip unsupported toolchains; retain compiler and assertion diagnostics."""
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A C++20 host compiler is required")
    probe = subprocess.run(
        [compiler, "-std=c++20", "-x", "c++", "-fsyntax-only", "-"],
        input="#include <atomic>\nstatic_assert(__cplusplus >= 202002L);\n",
        capture_output=True,
        text=True,
        check=False,
    )
    if probe.returncode:
        pytest.skip(f"C++20 host compiler unavailable: {probe.stderr}")
    # Inherit pytest's captured streams so failures show the actual diagnostics.
    subprocess.run([compiler, "-std=c++20", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
