"""Shared utilities for ESPHome integration tests - keeping output from failing tests."""

from __future__ import annotations

from pathlib import Path

#: Where a failing test leaves output for someone to look at afterwards. pytest's own
#: temporary folder is no use on a CI runner, which throws the whole workspace away when
#: the job ends; the workflow uploads this folder instead when a job fails.
ARTIFACT_DIR = Path(__file__).resolve().parents[2] / "test_artifacts"


def keep_artifact(name: str, data: bytes) -> Path:
    """Write ``data`` where it can still be read after the run, and return the path.

    Args:
        name: File name to write under the artifact folder.
        data: Contents to write.

    Returns:
        The full path written.
    """
    ARTIFACT_DIR.mkdir(parents=True, exist_ok=True)
    path = ARTIFACT_DIR / name
    path.write_bytes(data)
    return path
