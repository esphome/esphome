"""Print JSON download info for the ESP-IDF tools an install would fetch.

Run via ``python <this file> <idf_framework_root> <targets-csv> <tool-spec>...``.
PYTHONPATH must include ``<idf_framework_root>/tools`` so ``idf_tools`` is
importable, and IDF_TOOLS_PATH must be set. Prints a JSON list of
``{name, url, size, sha256, dest}`` for every tool version that is not yet
installed, where ``dest`` is the archive filename ``idf_tools.py install``
expects to find in ``<IDF_TOOLS_PATH>/dist``. Tools with no download for the
current platform are skipped; already-installed versions are skipped so a
pruned download cache is not re-fetched.

The target/tool expansion mirrors ``idf_tools.py install`` (targets passed to
``add_and_check_targets`` accumulate with idf-env.json) but nothing is saved
or written — this script only reports what the install would download.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

from contextlib import redirect_stdout
import json
import os
from pathlib import Path
import sys

from idf_tools import (
    CURRENT_PLATFORM,
    TOOLS_FILE,
    IDFEnv,
    ToolBinaryError,
    add_and_check_targets,
    expand_tools_arg,
    g,
    get_idf_download_url_apply_mirrors,
    load_tools_info,
)


def collect_downloads() -> list[dict]:
    g.idf_path = sys.argv[1]
    g.idf_tools_path = os.environ.get("IDF_TOOLS_PATH")
    g.tools_json = str(Path(g.idf_path) / TOOLS_FILE)

    targets = add_and_check_targets(IDFEnv.get_idf_env(), sys.argv[2])
    tools_info = load_tools_info()
    downloads: list[dict] = []

    for name in expand_tools_arg(sys.argv[3:], tools_info, targets):
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
            # A broken installed binary is idf_tools' problem to repair on
            # install; note it and treat the version as not installed.
            print(f"tool {name} failed its binary check: {e}", file=sys.stderr)
        if version in tool.versions_installed or version not in tool.versions:
            continue
        download = tool.versions[version].get_download_for_platform(CURRENT_PLATFORM)
        if download is None:
            continue
        downloads.append(
            {
                "name": f"{name}@{version}",
                # Apply the same IDF_MIRROR_PREFIX_MAP / IDF_GITHUB_ASSETS
                # rewriting the installer's own downloader applies, so users
                # behind a mirror prefetch from the mirror too.
                "url": get_idf_download_url_apply_mirrors(None, download.url),
                "size": download.size,
                "sha256": download.sha256,
                "dest": download.rename_dist or Path(download.url).name,
            }
        )
    return downloads


# idf_tools prints informational lines (e.g. mirror URL rewrites) to stdout;
# route them to stderr so stdout carries only the JSON result.
with redirect_stdout(sys.stderr):
    result = collect_downloads()
print(json.dumps(result))
