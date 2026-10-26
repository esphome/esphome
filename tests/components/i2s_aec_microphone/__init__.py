from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The host unit tests exercise only the header-only reference probe.
    # ESP-IDF microphone dependencies are covered by the firmware builds.
    manifest.dependencies = []
    manifest.auto_load = []
