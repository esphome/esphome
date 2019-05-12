#!/usr/bin/env python
import sys
import os.path

sys.path.append(os.path.dirname(__file__))
from helpers import build_all_include, build_compile_commands


def main():
    build_all_include()
    build_compile_commands()
    print("Done.")


if __name__ == '__main__':
    main()
