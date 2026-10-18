"""Run the production ring buffer against a bounded FreeRTOS API double."""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = []
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        sdk = Path(__file__).resolve().parent / "__sdk"
        cg.add_build_flag(f"-I{sdk}")

    manifest.to_code = to_code_testing
