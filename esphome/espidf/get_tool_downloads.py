"""Print JSON download info for the ESP-IDF tools an install would fetch.

Run via ``python <this file> <idf_framework_root> <targets-csv> <tool-spec>...``.
PYTHONPATH must include this directory (for ``_tool_resolution``) and
``<idf_framework_root>/tools`` (for ``idf_tools``), and IDF_TOOLS_PATH must
be set. Prints a JSON list of
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
import sys

from _tool_resolution import archive_name, init_idf_tools, iter_tool_downloads
from idf_tools import ToolBinaryError, get_idf_download_url_apply_mirrors


def collect_downloads() -> list[dict]:
    init_idf_tools(sys.argv[1])

    def on_broken(name: str, e: ToolBinaryError) -> bool:
        # A broken installed binary is idf_tools' problem to repair on
        # install; note it and treat the version as not installed.
        print(f"tool {name} failed its binary check: {e}", file=sys.stderr)
        return True

    return [
        {
            "name": f"{name}@{version}",
            # Apply the same IDF_MIRROR_PREFIX_MAP / IDF_GITHUB_ASSETS
            # rewriting the installer's own downloader applies, so users
            # behind a mirror prefetch from the mirror too.
            "url": get_idf_download_url_apply_mirrors(None, download.url),
            "size": download.size,
            "sha256": download.sha256,
            "dest": archive_name(download),
        }
        for _tool, name, version, download in iter_tool_downloads(
            sys.argv[2], sys.argv[3:], on_broken
        )
    ]


# idf_tools prints informational lines (e.g. mirror URL rewrites) to stdout;
# route them to stderr so stdout carries only the JSON result.
with redirect_stdout(sys.stderr):
    result = collect_downloads()
print(json.dumps(result))
