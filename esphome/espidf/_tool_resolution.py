"""Shared tool resolution for the sibling idf_tools-backed scripts.

Importable because ``python <script>`` puts this directory first on
sys.path; ``idf_tools`` itself comes from PYTHONPATH.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

from collections.abc import Callable, Iterator
import os
from pathlib import Path

from idf_tools import (
    CURRENT_PLATFORM,
    TOOLS_FILE,
    IDFEnv,
    ToolBinaryError,
    add_and_check_targets,
    expand_tools_arg,
    g,
    load_tools_info,
)


def init_idf_tools(idf_framework_root: str) -> None:
    """Point idf_tools' globals at the framework and IDF_TOOLS_PATH."""
    g.idf_path = idf_framework_root
    g.idf_tools_path = os.environ.get("IDF_TOOLS_PATH")
    g.tools_json = str(Path(g.idf_path) / TOOLS_FILE)


def archive_name(download: object) -> str:
    """The dist/ filename idf_tools downloads and installs this from."""
    return download.rename_dist or Path(download.url).name


def iter_tool_downloads(
    targets_csv: str,
    tool_specs: list[str],
    on_broken: Callable[[str, ToolBinaryError], bool],
) -> Iterator[tuple[object, str, str, object]]:
    """Yield (tool, name, version, download) per uninstalled tool, mirroring
    ``idf_tools.py install``'s expansion; ``on_broken(name, err)`` returns
    True to treat a tool with a failing installed binary as not installed."""
    targets = add_and_check_targets(IDFEnv.get_idf_env(), targets_csv)
    tools_info = load_tools_info()
    for name in expand_tools_arg(tool_specs, tools_info, targets):
        if "@" in name:
            name, version = name.split("@", 1)
        else:
            version = None
        tool = tools_info.get(name)
        if tool is None or not tool.compatible_with_platform():
            continue
        version = version or tool.get_recommended_version()
        if version is None:
            continue
        try:
            tool.find_installed_versions()
        except ToolBinaryError as e:
            if not on_broken(name, e):
                continue
        if version in tool.versions_installed or version not in tool.versions:
            continue
        download = tool.versions[version].get_download_for_platform(CURRENT_PLATFORM)
        if download is None:
            continue
        yield tool, name, version, download
