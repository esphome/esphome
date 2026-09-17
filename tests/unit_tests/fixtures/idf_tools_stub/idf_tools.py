"""Minimal idf_tools stand-in for the espidf helper-script tests."""

from collections.abc import Iterable
import os
import pathlib

CURRENT_PLATFORM = "linux-amd64"
TOOLS_FILE = "tools/tools.json"


class ToolBinaryError(RuntimeError):
    pass


class _G:
    idf_path: str | None = None
    idf_tools_path: str | None = None
    tools_json: str | None = None


g = _G()


class IDFEnv:
    @classmethod
    def get_idf_env(cls) -> "IDFEnv":
        return cls()


def add_and_check_targets(idf_env_obj: IDFEnv, targets_str: str) -> list[str]:
    return targets_str.split(",")


class _Download:
    def __init__(self, url: str, size: int, sha256: str, rename_dist: str = "") -> None:
        self.url = url
        self.size = size
        self.sha256 = sha256
        self.rename_dist = rename_dist


class _Version:
    def __init__(self, download: _Download | None) -> None:
        self._download = download

    def get_download_for_platform(self, platform_name: str) -> _Download | None:
        return self._download


class _Tool:
    def __init__(
        self,
        versions: dict[str, _Version],
        recommended: str | None,
        installed: Iterable[str] = (),
        broken: bool = False,
    ) -> None:
        self.name = ""  # filled in from the _TOOLS key below
        self.versions = versions
        self._recommended = recommended
        self.versions_installed = list(installed)
        self._broken = broken

    def compatible_with_platform(self) -> bool:
        return True

    def get_recommended_version(self) -> str | None:
        return self._recommended

    def find_installed_versions(self) -> None:
        if self._broken:
            raise ToolBinaryError("broken binary")

    def get_path_for_version(self, version: str) -> str:
        return str(pathlib.Path(g.idf_tools_path) / "tools" / self.name / version)

    def install(self, version: str) -> None:
        dest = pathlib.Path(self.get_path_for_version(version))
        dest.mkdir(exist_ok=True, parents=True)
        if self.name in os.environ.get("TEST_FAIL_INSTALL", "").split(","):
            # Fail mid-install like a torn unpack: the partial dir is left
            # behind and check_binary_valid's failure path exits the process
            (dest / ".partial").write_text("torn", encoding="utf-8")
            raise SystemExit(1)
        (dest / ".installed").write_text("ok", encoding="utf-8")


_TOOLS = {
    "cmake": _Tool(
        {"3.30.2": _Version(_Download("https://gh.test/cmake.tar.gz", 11, "aa"))},
        "3.30.2",
    ),
    "ninja": _Tool(
        {
            "1.12.1": _Version(
                _Download("https://gh.test/ninja-mac.zip", 22, "bb", "ninja-v1.zip")
            )
        },
        "1.12.1",
    ),
    "installed-tool": _Tool(
        {"1.0": _Version(_Download("https://gh.test/x.tar.gz", 33, "cc"))},
        "1.0",
        installed=["1.0"],
    ),
    "broken-tool": _Tool(
        {"2.0": _Version(_Download("https://gh.test/y.tar.gz", 44, "dd"))},
        "2.0",
        broken=True,
    ),
    "no-recommended-tool": _Tool({"3.0": _Version(None)}, None),
    "no-download-tool": _Tool({"4.0": _Version(None)}, "4.0"),
}

for _name, _tool in _TOOLS.items():
    _tool.name = _name


def load_tools_info() -> dict[str, _Tool]:
    # Test hook: strip verification metadata from the named tools
    for name in os.environ.get("TEST_NO_SHA", "").split(","):
        if (tool := _TOOLS.get(name)) is not None:
            for version in tool.versions.values():
                if (download := version.get_download_for_platform("")) is not None:
                    download.sha256 = ""
    return _TOOLS


def expand_tools_arg(
    tools_spec: list[str], overall_tools: dict[str, _Tool], targets: list[str]
) -> list[str]:
    if "required" in tools_spec:
        return list(overall_tools)
    return [t for t in tools_spec if "@" not in t] + [t for t in tools_spec if "@" in t]


def get_idf_download_url_apply_mirrors(
    args: object = None, download_url: str = ""
) -> str:
    print(f"Changed download URL: {download_url}")  # noise on stdout, like idf_tools
    prefix = os.environ.get("TEST_MIRROR_PREFIX")
    if prefix:
        return prefix + download_url
    return download_url
