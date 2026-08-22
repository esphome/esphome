from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The scan list helper is header-only. The real dependencies (wifi,
    # web_server_base, ota.web_server) do not build on the host, so drop them
    # from the unit test build.
    manifest.dependencies = []
    manifest.auto_load = []
