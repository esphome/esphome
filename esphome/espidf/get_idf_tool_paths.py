"""Print JSON ``{paths_to_export, export_vars}`` for ESP-IDF tools.

Run via ``python <this file> <idf_framework_root>``. PYTHONPATH must include
``<idf_framework_root>/tools`` so ``idf_tools`` is importable. Exits with
status 1 and prints ``Missing ESP-IDF tools: ...`` on stderr if any tool is
not installed.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

import json
import os
import sys
from types import SimpleNamespace

from idf_tools import (
    TOOLS_FILE,
    IDFEnv,
    IDFTool,
    filter_tools_info,
    g,
    load_tools_info,
    process_tool,
)

g.idf_path = sys.argv[1]
g.idf_tools_path = os.environ.get("IDF_TOOLS_PATH")
g.tools_json = os.path.join(g.idf_path, TOOLS_FILE)

tools_info = filter_tools_info(IDFEnv.get_idf_env(), load_tools_info())
args = SimpleNamespace(prefer_system=False)
paths_to_export: list[str] = []
export_vars: dict[str, str] = {}
missing_tools: list[str] = []

for name, tool in tools_info.items():
    if tool.get_install_type() == IDFTool.INSTALL_NEVER:
        continue
    tool_paths, tool_vars, found = process_tool(
        tool, name, args, "install_cmd", "prefer_system_hint"
    )
    if not found:
        missing_tools.append(name)
    paths_to_export += tool_paths
    export_vars |= tool_vars

if missing_tools:
    print("Missing ESP-IDF tools: " + ", ".join(missing_tools), file=sys.stderr)
    raise SystemExit(1)

print(json.dumps({"paths_to_export": paths_to_export, "export_vars": export_vars}))
