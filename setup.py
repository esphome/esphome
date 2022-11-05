#!/usr/bin/env python3
"""esphome setup script."""
import os

from setuptools import setup, find_packages

from esphome import const

PROJECT_NAME = "esphome"
PROJECT_PACKAGE_NAME = "esphome"
PROJECT_LICENSE = "MIT"
PROJECT_AUTHOR = "ESPHome"
PROJECT_COPYRIGHT = "2019, ESPHome"
PROJECT_URL = "https://esphome.io/"
PROJECT_EMAIL = "esphome@nabucasa.com"

PROJECT_GITHUB_USERNAME = "esphome"
PROJECT_GITHUB_REPOSITORY = "esphome"

PYPI_URL = f"https://pypi.python.org/pypi/{PROJECT_PACKAGE_NAME}"
GITHUB_PATH = f"{PROJECT_GITHUB_USERNAME}/{PROJECT_GITHUB_REPOSITORY}"
GITHUB_URL = f"https://github.com/{GITHUB_PATH}"

DOWNLOAD_URL = f"{GITHUB_URL}/archive/{const.__version__}.zip"

here = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join(here, "requirements.txt")) as requirements_txt:
    REQUIRES = requirements_txt.read().splitlines()

with open(os.path.join(here, "README.md")) as readme:
    LONG_DESCRIPTION = readme.read()

# If you have problems importing platformio and esptool as modules you can set
# $ESPHOME_USE_SUBPROCESS to make ESPHome call their executables instead.
# This means they have to be in your $PATH.
if "ESPHOME_USE_SUBPROCESS" in os.environ:
    # Remove platformio and esptool from requirements
    REQUIRES = [
        req
        for req in REQUIRES
        if not any(req.startswith(prefix) for prefix in ["platformio", "esptool"])
    ]

CLASSIFIERS = [
    "Environment :: Console",
    "Intended Audience :: Developers",
    "Intended Audience :: End Users/Desktop",
    "License :: OSI Approved :: MIT License",
    "Programming Language :: C++",
    "Programming Language :: Python :: 3",
    "Topic :: Home Automation",
]

setup(
    name=PROJECT_PACKAGE_NAME,
    version=const.__version__,
    license=PROJECT_LICENSE,
    url=GITHUB_URL,
    project_urls={
        "Bug Tracker": "https://github.com/esphome/issues/issues",
        "Feature Request Tracker": "https://github.com/esphome/feature-requests/issues",
        "Source Code": "https://github.com/esphome/esphome",
        "Documentation": "https://esphome.io",
        "Twitter": "https://twitter.com/esphome_",
    },
    download_url=DOWNLOAD_URL,
    author=PROJECT_AUTHOR,
    author_email=PROJECT_EMAIL,
    description="Make creating custom firmwares for ESP32/ESP8266 super easy.",
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    include_package_data=True,
    zip_safe=False,
    platforms="any",
    test_suite="tests",
    python_requires=">=3.9.0",
    install_requires=REQUIRES,
    keywords=["home", "automation"],
    entry_points={"console_scripts": ["esphome = esphome.__main__:main"]},
    packages=find_packages(include="esphome.*"),
)
