"""Compile the streaming model owner with a host-only inference-engine double."""

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
        cg.add_build_flag(f"-I{CORE.relative_src_path('micro_wake_word', '__sdk')}")

    manifest.to_code = to_code_testing
