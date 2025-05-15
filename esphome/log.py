from enum import Enum
import logging

from esphome.core import CORE


class AnsiFore(Enum):
    KEEP = ""
    BLACK = "\033[30m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    RESET = "\033[39m"

    BOLD_BLACK = "\033[1;30m"
    BOLD_RED = "\033[1;31m"
    BOLD_GREEN = "\033[1;32m"
    BOLD_YELLOW = "\033[1;33m"
    BOLD_BLUE = "\033[1;34m"
    BOLD_MAGENTA = "\033[1;35m"
    BOLD_CYAN = "\033[1;36m"
    BOLD_WHITE = "\033[1;37m"
    BOLD_RESET = "\033[1;39m"


class AnsiStyle(Enum):
    BRIGHT = "\033[1m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    THIN = "\033[2m"
    NORMAL = "\033[22m"
    RESET_ALL = "\033[0m"


def color(col: AnsiFore, msg: str, reset: bool = True) -> str:
    s = col.value + msg
    if reset and col:
        s += AnsiStyle.RESET_ALL.value
    return s


class ESPHomeLogFormatter(logging.Formatter):
    def __init__(self, *, include_timestamp: bool):
        fmt = "%(asctime)s " if include_timestamp else ""
        fmt += "%(levelname)s %(message)s"
        super().__init__(fmt=fmt, style="%")

    # @override
    def format(self, record: logging.LogRecord) -> str:
        formatted = super().format(record)
        prefix = {
            "DEBUG": AnsiFore.CYAN.value,
            "INFO": AnsiFore.GREEN.value,
            "WARNING": AnsiFore.YELLOW.value,
            "ERROR": AnsiFore.RED.value,
            "CRITICAL": AnsiFore.RED.value,
        }.get(record.levelname, "")
        return f"{prefix}{formatted}{AnsiStyle.RESET_ALL.value}"


def setup_log(
    log_level: int = logging.INFO,
    include_timestamp: bool = False,
) -> None:
    import colorama

    colorama.init()

    # Setup logging - will map log level from string to constant
    logging.basicConfig(level=log_level)

    if logging.root.level == logging.DEBUG:
        CORE.verbose = True
    elif logging.root.level == logging.CRITICAL:
        CORE.quiet = True

    logging.getLogger("urllib3").setLevel(logging.WARNING)

    logging.getLogger().handlers[0].setFormatter(
        ESPHomeLogFormatter(include_timestamp=include_timestamp)
    )
