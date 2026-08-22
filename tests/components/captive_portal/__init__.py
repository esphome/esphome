from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The scan list helper is header-only and needs none of the component's real
    # dependencies. Pulling them in breaks the host build: web_server_base
    # includes ESPAsyncWebServer.h and ota.web_server includes md5/md5.h, neither
    # of which exists there.
    manifest.dependencies = []
    manifest.auto_load = []
