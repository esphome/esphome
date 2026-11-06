from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Only the GPIO-independent model is exercised by the real host gtest build.
    manifest.dependencies = []
