"""Load the RX8111 time platform's dependencies in the C++ test build."""

from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = manifest.dependencies + ["i2c", "time"]
