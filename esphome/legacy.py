import sys


def main():
    print("The esphomeyaml command has been renamed to esphome.")
    print("")
    print("$ esphome {}".format(" ".join(sys.argv[1:])))
    return 1


if __name__ == "__main__":
    sys.exit(main())
