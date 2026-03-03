#!/usr/bin/env python3
"""Add metadata to memory analysis JSON file.

This script adds components and platform metadata to an existing
memory analysis JSON file. Used by CI to ensure all required fields are present
for the comment script.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Add metadata to memory analysis JSON file"
    )
    parser.add_argument(
        "--json-file",
        required=True,
        help="Path to JSON file to update",
    )
    parser.add_argument(
        "--components",
        required=True,
        help='JSON array of component names (e.g., \'["api", "wifi"]\')',
    )
    parser.add_argument(
        "--platform",
        required=True,
        help="Platform name",
    )

    args = parser.parse_args()

    # Load existing JSON
    json_path = Path(args.json_file)
    if not json_path.exists():
        print(f"Error: JSON file not found: {args.json_file}", file=sys.stderr)
        return 1

    try:
        with open(json_path, encoding="utf-8") as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        print(f"Error loading JSON: {e}", file=sys.stderr)
        return 1

    # Parse components
    try:
        components = json.loads(args.components)
        if not isinstance(components, list):
            print("Error: --components must be a JSON array", file=sys.stderr)
            return 1
        # Element-level validation: ensure each component is a non-empty string
        for idx, comp in enumerate(components):
            if not isinstance(comp, str) or not comp.strip():
                print(
                    f"Error: component at index {idx} is not a non-empty string: {comp!r}",
                    file=sys.stderr,
                )
                return 1
    except json.JSONDecodeError as e:
        print(f"Error parsing components: {e}", file=sys.stderr)
        return 1

    # Add metadata
    data["components"] = components
    data["platform"] = args.platform

    # Write back
    try:
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        print(f"Added metadata to {args.json_file}", file=sys.stderr)
    except OSError as e:
        print(f"Error writing JSON: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
