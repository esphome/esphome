"""Include the real packet-transport adapter in radio regression tests."""

from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = manifest.dependencies + [
        "packet_transport",
        "sensor",
        "binary_sensor",
    ]
    manifest.add_resources(
        FileResource(
            "esphome.components.sx126x", "packet_transport/sx126x_transport.h"
        ),
        FileResource(
            "esphome.components.sx126x", "packet_transport/sx126x_transport.cpp"
        ),
    )
