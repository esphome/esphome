"""Build the response state machine with host substitutes for ESP32 buffers."""

import esphome.codegen as cg
from esphome.core import CORE
from esphome.loader import get_component
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride, set_testing_manifest


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_VOICE_ASSISTANT")
        CORE.testing_ensure_platform_registered("media_player")
        CORE.testing_ensure_platform_registered("speaker")
        cg.add_build_flag(
            f"-include {CORE.relative_src_path('voice_assistant/host_audio.h')}"
        )

    manifest.to_code = to_code_testing
    manifest.auto_load = [*manifest.auto_load, "media_player", "speaker"]
    microphone = ComponentManifestOverride(get_component("microphone"))
    microphone.to_code = None
    microphone.resources = [
        resource
        for resource in microphone.resources
        if resource.resource != "microphone_source.cpp"
    ]
    set_testing_manifest("microphone", microphone)
