"""Unit tests for docker/build.py command generation."""

import importlib.util
from pathlib import Path
import sys

import pytest

_BUILD_PY = Path(__file__).parents[2] / "docker" / "build.py"
_spec = importlib.util.spec_from_file_location("docker_build", _BUILD_PY)
docker_build = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(docker_build)


def _run(capsys: pytest.CaptureFixture[str], *argv: str) -> list[str]:
    """Run build.py main() in dry-run mode and return the emitted commands."""
    full_argv = ["build.py", "--dry-run", *argv]
    with pytest.MonkeyPatch.context() as mp:
        mp.setattr(sys, "argv", full_argv)
        docker_build.main()
    out = capsys.readouterr().out
    return [line[2:] for line in out.splitlines() if line.startswith("$ ")]


def test_branch_build_pushes_single_ghcr_tag_without_cache_to(
    capsys: pytest.CaptureFixture[str],
) -> None:
    commands = _run(
        capsys,
        "--tag",
        "my-branch",
        "--arch",
        "amd64",
        "--build-type",
        "docker",
        "--registry",
        "ghcr",
        "build",
        "--push",
        "--no-cache-to",
    )

    assert len(commands) == 1
    cmd = commands[0]
    # Custom tag -> only the tag itself, no companion "dev"/"latest" tags
    assert "--tag ghcr.io/esphome/esphome-amd64:my-branch" in cmd
    assert ":dev" not in cmd
    # ghcr only -> no Docker Hub image name
    assert "--tag esphome/esphome-amd64:my-branch" not in cmd
    # custom tag falls back to the dev cache for reads
    assert (
        "--cache-from type=registry,ref=ghcr.io/esphome/esphome-amd64:cache-dev" in cmd
    )
    assert "--push" in cmd
    # --no-cache-to must suppress the cache write
    assert "--cache-to" not in cmd


def test_branch_manifest_targets_ghcr_only(
    capsys: pytest.CaptureFixture[str],
) -> None:
    commands = _run(
        capsys,
        "--tag",
        "my-branch",
        "--build-type",
        "ha-addon",
        "--registry",
        "ghcr",
        "manifest",
    )

    assert commands == [
        "docker buildx imagetools create "
        "--tag ghcr.io/esphome/esphome-hassio:my-branch "
        "ghcr.io/esphome/esphome-hassio-amd64:my-branch "
        "ghcr.io/esphome/esphome-hassio-aarch64:my-branch"
    ]


def test_release_build_keeps_both_registries_and_cache_to(
    capsys: pytest.CaptureFixture[str],
) -> None:
    commands = _run(
        capsys,
        "--tag",
        "2025.6.0",
        "--arch",
        "amd64",
        "--build-type",
        "docker",
        "build",
        "--push",
    )

    cmd = commands[0]
    # Default (no --registry) keeps both Docker Hub and ghcr image names
    assert "--tag esphome/esphome-amd64:2025.6.0" in cmd
    assert "--tag ghcr.io/esphome/esphome-amd64:2025.6.0" in cmd
    # Release channel still gets its companion tags
    assert "--tag esphome/esphome-amd64:latest" in cmd
    # Without --no-cache-to the cache write is preserved
    assert (
        "--cache-to type=registry,ref=ghcr.io/esphome/esphome-amd64:cache-latest,mode=max"
        in cmd
    )


def test_build_no_push_omits_push_and_cache(
    capsys: pytest.CaptureFixture[str],
) -> None:
    commands = _run(
        capsys,
        "--tag",
        "my-branch",
        "--arch",
        "amd64",
        "--build-type",
        "docker",
        "--registry",
        "ghcr",
        "build",
    )

    cmd = commands[0]
    assert "--tag ghcr.io/esphome/esphome-amd64:my-branch" in cmd
    assert "--push" not in cmd
    assert "--cache-to" not in cmd


def test_build_dockerhub_only(capsys: pytest.CaptureFixture[str]) -> None:
    commands = _run(
        capsys,
        "--tag",
        "my-branch",
        "--arch",
        "amd64",
        "--build-type",
        "docker",
        "--registry",
        "dockerhub",
        "build",
        "--push",
    )

    cmd = commands[0]
    assert "--tag esphome/esphome-amd64:my-branch" in cmd
    assert "ghcr.io" not in cmd
    # Cache reference falls back to Docker Hub when GHCR isn't selected
    assert "--cache-from type=registry,ref=esphome/esphome-amd64:cache-dev" in cmd


def test_manifest_dockerhub_only(capsys: pytest.CaptureFixture[str]) -> None:
    commands = _run(
        capsys,
        "--tag",
        "my-branch",
        "--build-type",
        "docker",
        "--registry",
        "dockerhub",
        "manifest",
    )

    create = commands[0]
    assert create.startswith(
        "docker buildx imagetools create --tag esphome/esphome:my-branch "
    )
    assert "ghcr.io" not in create
