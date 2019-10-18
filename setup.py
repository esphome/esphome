#!/usr/bin/env python
"""esphome setup script."""
from setuptools import setup, find_packages
import os

from esphome import const

PROJECT_NAME = 'esphome'
PROJECT_PACKAGE_NAME = 'esphome'
PROJECT_LICENSE = 'MIT'
PROJECT_AUTHOR = 'ESPHome'
PROJECT_COPYRIGHT = '2019, ESPHome'
PROJECT_URL = 'https://esphome.io/'
PROJECT_EMAIL = 'contact@esphome.io'

PROJECT_GITHUB_USERNAME = 'esphome'
PROJECT_GITHUB_REPOSITORY = 'esphome'

PYPI_URL = 'https://pypi.python.org/pypi/{}'.format(PROJECT_PACKAGE_NAME)
GITHUB_PATH = '{}/{}'.format(PROJECT_GITHUB_USERNAME, PROJECT_GITHUB_REPOSITORY)
GITHUB_URL = 'https://github.com/{}'.format(GITHUB_PATH)

DOWNLOAD_URL = '{}/archive/v{}.zip'.format(GITHUB_URL, const.__version__)

REQUIRES = [
    'voluptuous==0.11.7',
    'PyYAML==5.1.2',
    'paho-mqtt==1.4.0',
    'colorlog==4.0.2',
    'tornado==5.1.1',
    'typing>=3.6.6;python_version<"3.5"',
    'protobuf==3.10.0',
    'tzlocal==2.0.0',
    'pytz==2019.3',
    'pyserial==3.4',
    'ifaddr==0.1.6',
]

# If you have problems importing platformio and esptool as modules you can set
# $ESPHOME_USE_SUBPROCESS to make ESPHome call their executables instead.
# This means they have to be in your $PATH.
if os.environ.get('ESPHOME_USE_SUBPROCESS') is None:
    REQUIRES.extend([
        'platformio==4.0.3',
        'esptool==2.7',
    ])

CLASSIFIERS = [
    'Environment :: Console',
    'Intended Audience :: Developers',
    'Intended Audience :: End Users/Desktop',
    'License :: OSI Approved :: MIT License',
    'Programming Language :: C++',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 3',
    'Topic :: Home Automation',
]

setup(
    name=PROJECT_PACKAGE_NAME,
    version=const.__version__,
    license=PROJECT_LICENSE,
    url=GITHUB_URL,
    download_url=DOWNLOAD_URL,
    author=PROJECT_AUTHOR,
    author_email=PROJECT_EMAIL,
    description="Make creating custom firmwares for ESP32/ESP8266 super easy.",
    include_package_data=True,
    zip_safe=False,
    platforms='any',
    test_suite='tests',
    python_requires='>=2.7,!=3.0.*,!=3.1.*,!=3.2.*,!=3.3.*,!=3.4.*,<4.0',
    install_requires=REQUIRES,
    keywords=['home', 'automation'],
    entry_points={
        'console_scripts': [
            'esphome = esphome.__main__:main'
        ]
    },
    packages=find_packages()
)
