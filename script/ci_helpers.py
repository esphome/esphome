"""Common helper functions for CI scripts."""

from __future__ import annotations

import os


def write_github_output(outputs: dict[str, str | int]) -> None:
    """Write multiple outputs to GITHUB_OUTPUT or stdout.

    When running in GitHub Actions, writes to the GITHUB_OUTPUT file.
    When running locally, writes to stdout for debugging.

    Args:
        outputs: Dictionary of key-value pairs to write
    """
    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as f:
            f.writelines(f"{key}={value}\n" for key, value in outputs.items())
    else:
        for key, value in outputs.items():
            print(f"{key}={value}")
