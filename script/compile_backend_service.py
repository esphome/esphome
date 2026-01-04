"""Compile backend utility for processing ESPHome configs from S3.

This script downloads a YAML configuration from a public S3 object URL,
validates it with ESPHome, reports the result to an HTTP API, compiles the
firmware, uploads the resulting factory image to S3, and notifies a completion
endpoint. It also cleans up build artifacts while leaving cached packages in
place to speed up future runs.

Mock S3 targets such as Beeceptor are supported via ``--s3-endpoint`` together
with ``--s3-path-style`` and ``--s3-unsigned``. See
https://beeceptor.com/docs/tutorials/aws-s3-mock-server/ for setup details.
"""

from __future__ import annotations

import argparse
from collections.abc import Iterable
import logging
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlparse
import urllib.request

import boto3
from botocore import UNSIGNED
from botocore.config import Config
import botocore.exceptions
import requests

_LOGGER = logging.getLogger(__name__)


class CompilationError(RuntimeError):
    """Raised when validation or compilation fails."""


class ArtifactNotFound(RuntimeError):
    """Raised when the factory firmware image cannot be located."""


def _post_json(url: str | None, payload: dict) -> None:
    """Send a JSON payload if a callback URL is provided."""

    if url is None:
        return

    _LOGGER.debug("Posting JSON to %s: %s", url, payload)
    response = requests.post(url, json=payload, timeout=30)
    response.raise_for_status()


def _download(url: str, destination: Path) -> None:
    """Download a file from a public URL to destination."""

    destination.parent.mkdir(parents=True, exist_ok=True)
    _LOGGER.info("Downloading configuration from %s", url)
    with urllib.request.urlopen(url) as response, destination.open("wb") as handle:
        shutil.copyfileobj(response, handle)


def _run_cli(command: Iterable[str], workdir: Path) -> tuple[int, str]:
    """Run a command and return its exit code and combined output."""

    _LOGGER.debug("Running command: %s", " ".join(command))
    process = subprocess.run(
        list(command),
        cwd=workdir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    output = process.stdout
    _LOGGER.debug("Command output (%s):\n%s", process.returncode, output)
    return process.returncode, output


def _derive_output_name(stem: str) -> str:
    """Derive the upload name for the compiled binary.

    If the source name ends with a trailing ``1`` (for example ``xyz1111``), the
    trailing character is stripped before appending ``.bin`` so the output would
    become ``xyz111.bin``. In all other cases the full stem is used.
    """

    trimmed = stem.removesuffix("1")
    return f"{trimmed}.bin"


def _find_factory_image(build_root: Path) -> Path:
    """Locate the generated factory image inside the build directory."""

    matches = list(build_root.rglob("firmware.factory.bin"))
    if not matches:
        raise ArtifactNotFound(f"Unable to locate firmware.factory.bin in {build_root}")
    # Prefer the most recently modified file to handle multiple environments
    matches.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    return matches[0]


def _upload_factory_image(
    image_path: Path,
    bucket: str,
    key: str,
    region: str | None,
    endpoint: str | None,
    path_style: bool,
    unsigned: bool,
) -> None:
    """Upload the factory image to S3 using boto3.

    ``path_style`` and ``unsigned`` make it possible to target S3-compatible
    mock servers such as Beeceptor, which do not always support virtual-hosted
    buckets or AWS Signature v4.
    """

    session_kwargs: dict = {}
    client_kwargs: dict = {}
    if region:
        session_kwargs["region_name"] = region
    if endpoint:
        client_kwargs["endpoint_url"] = endpoint

    config_kwargs: dict = {}
    if path_style:
        config_kwargs["s3"] = {"addressing_style": "path"}
    if unsigned:
        config_kwargs["signature_version"] = UNSIGNED
    if config_kwargs:
        client_kwargs["config"] = Config(**config_kwargs)

    session = boto3.session.Session(**session_kwargs)
    s3 = session.client("s3", **client_kwargs)
    _LOGGER.info("Uploading %s to s3://%s/%s", image_path, bucket, key)
    try:
        s3.upload_file(str(image_path), bucket, key)
    except botocore.exceptions.BotoCoreError as err:
        raise CompilationError(f"Failed to upload firmware to S3: {err}") from err


def _cleanup(temp_dir: Path, build_dir: Path) -> None:
    """Remove temporary files while keeping cached toolchains intact."""

    _LOGGER.info("Cleaning up temporary data")
    shutil.rmtree(temp_dir, ignore_errors=True)
    shutil.rmtree(build_dir, ignore_errors=True)


def _notify_stage(
    url: str | None,
    config_url: str,
    stage: str,
    success: bool,
    output: str,
    extra: dict | None = None,
) -> None:
    payload = {
        "config_url": config_url,
        "stage": stage,
        "success": success,
        "output": output,
    }
    if extra:
        payload.update(extra)
    _post_json(url, payload)


def process_configuration(args: argparse.Namespace) -> None:
    workspace = Path(args.workspace).resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    temp_dir = Path(tempfile.mkdtemp(prefix="esphome-compile-"))

    parsed = urlparse(args.config_url)
    yaml_name = Path(parsed.path).name
    yaml_path = temp_dir / yaml_name
    build_dir = yaml_path.parent / "build" / yaml_path.stem

    try:
        _download(args.config_url, yaml_path)
        validation_code, validation_output = _run_cli(
            [sys.executable, "-m", "esphome", "config", str(yaml_path)], workspace
        )
        if validation_code != 0:
            _notify_stage(
                args.status_callback,
                args.config_url,
                "validation",
                False,
                validation_output,
            )
            raise CompilationError("Configuration validation failed")

        _notify_stage(
            args.status_callback,
            args.config_url,
            "validation",
            True,
            validation_output,
        )

        compile_code, compile_output = _run_cli(
            [sys.executable, "-m", "esphome", "compile", str(yaml_path)], workspace
        )
        if compile_code != 0:
            _notify_stage(
                args.status_callback,
                args.config_url,
                "compile",
                False,
                compile_output,
            )
            raise CompilationError("Compilation failed")

        _notify_stage(
            args.status_callback,
            args.config_url,
            "compile",
            True,
            compile_output,
        )

        factory_image = _find_factory_image(build_dir)
        upload_name = _derive_output_name(yaml_path.stem)
        upload_key = (
            str(Path(args.upload_prefix) / upload_name)
            if args.upload_prefix
            else upload_name
        )
        _upload_factory_image(
            factory_image,
            args.upload_bucket,
            upload_key,
            args.aws_region,
            args.s3_endpoint,
            args.s3_path_style,
            args.s3_unsigned,
        )

        _notify_stage(
            args.completion_callback,
            args.config_url,
            "complete",
            True,
            compile_output,
            extra={
                "uploaded_key": upload_key,
                "bucket": args.upload_bucket,
            },
        )
    finally:
        _cleanup(temp_dir, build_dir)


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ESPHome compilation backend service")
    parser.add_argument(
        "config_url", help="Public S3 URL pointing to the YAML configuration"
    )
    parser.add_argument(
        "upload_bucket", help="S3 bucket to upload compiled firmware to"
    )
    parser.add_argument(
        "--upload-prefix",
        help="Key prefix within the upload bucket for the compiled binary",
        default="",
    )
    parser.add_argument(
        "--status-callback",
        help="HTTP endpoint to receive validation/compile status payloads",
    )
    parser.add_argument(
        "--completion-callback",
        help="HTTP endpoint to receive completion notification payloads",
    )
    parser.add_argument(
        "--workspace",
        help="Working directory where builds will be executed",
        default=Path.cwd(),
        type=Path,
    )
    parser.add_argument(
        "--aws-region",
        help="AWS region for the upload bucket",
    )
    parser.add_argument(
        "--s3-endpoint",
        help="Custom S3 endpoint (useful for self-hosted or proxied environments)",
    )
    parser.add_argument(
        "--s3-path-style",
        action="store_true",
        help="Force path-style S3 addressing (required by some mock endpoints like Beeceptor)",
    )
    parser.add_argument(
        "--s3-unsigned",
        action="store_true",
        help="Disable request signing for S3 uploads (useful for public/mock endpoints)",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Log level for the backend runner",
    )
    return parser


def main() -> int:
    parser = build_argument_parser()
    args = parser.parse_args()
    logging.basicConfig(level=getattr(logging, args.log_level))

    try:
        process_configuration(args)
    except (CompilationError, ArtifactNotFound, requests.RequestException) as err:
        _LOGGER.error("%s", err)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
