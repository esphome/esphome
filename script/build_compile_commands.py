#!/usr/bin/env python
from .helpers import build_all_include, build_compile_commands


def main():
    build_all_include()
    build_compile_commands()
    print("Done.")


if __name__ == '__main__':
    main()
