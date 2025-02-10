#!/usr/bin/env python3

import os
from pathlib import Path
import subprocess
import sys


def find_and_activate_virtualenv():
    if (
        ("VIRTUAL_ENV" in os.environ)
        or os.environ.get("DEVCONTAINER", False)
        or os.environ.get("ESPHOME_NO_VENV", False)
    ):
        return

    try:
        # Get the top-level directory of the git repository
        my_path = subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"], text=True
        ).strip()
    except subprocess.CalledProcessError:
        print(
            "Error: Not a git repository or unable to determine the top-level directory.",
            file=sys.stderr,
        )
        return

    # Check for virtual environments
    for venv in ["venv", ".venv", "."]:
        activate_path = (
            Path(my_path)
            / venv
            / ("Scripts" if os.name == "nt" else "bin")
            / "activate"
        )
        if activate_path.exists():
            # Activate the virtual environment by updating PATH
            venv_bin_dir = activate_path.parent
            os.environ["PATH"] = f"{venv_bin_dir}{os.pathsep}{os.environ['PATH']}"
            os.environ["VIRTUAL_ENV"] = str(venv_bin_dir.parent)
            print(f"Activated virtual environment: {venv_bin_dir.parent}")
            return

    print("No virtual environment found.", file=sys.stderr)


def run_command():
    # Execute the remaining arguments in the new environment
    if len(sys.argv) > 1:
        subprocess.run(sys.argv[1:], check=False)
    else:
        print(
            "No command provided to run in the virtual environment.",
            file=sys.stderr,
        )


if __name__ == "__main__":
    find_and_activate_virtualenv()
    run_command()
