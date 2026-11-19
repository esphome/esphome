from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Copy the startup helper without loading the ESP32 media player."""
    manifest.add_resources(
        FileResource("esphome.components.speaker", "media_player/playback_start.h")
    )
