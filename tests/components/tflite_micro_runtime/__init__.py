"""Compile the runtime result lifecycle with host-only inference-engine and platform doubles."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["host"]
    manifest.auto_load = []
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_build_flag(
            f"-I{CORE.relative_src_path('tflite_micro_runtime', '__sdk')}"
        )

    manifest.to_code = to_code_testing
