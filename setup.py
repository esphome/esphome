#!/usr/bin/env python
"""esphomeyaml setup script."""
from setuptools import setup, find_packages

from esphomeyaml import const

PROJECT_NAME = 'esphomeyaml'
PROJECT_PACKAGE_NAME = 'esphomeyaml'
PROJECT_LICENSE = 'MIT'
PROJECT_AUTHOR = 'Otto Winter'
PROJECT_COPYRIGHT = '2018, Otto Winter'
PROJECT_URL = 'https://esphomelib.com/esphomeyaml/index.html'
PROJECT_EMAIL = 'contact@otto-winter.com'

PROJECT_GITHUB_USERNAME = 'OttoWinter'
PROJECT_GITHUB_REPOSITORY = 'esphomeyaml'

PYPI_URL = 'https://pypi.python.org/pypi/{}'.format(PROJECT_PACKAGE_NAME)
GITHUB_PATH = '{}/{}'.format(PROJECT_GITHUB_USERNAME, PROJECT_GITHUB_REPOSITORY)
GITHUB_URL = 'https://github.com/{}'.format(GITHUB_PATH)

DOWNLOAD_URL = '{}/archive/{}.zip'.format(GITHUB_URL, const.__version__)

REQUIRES = [
    'voluptuous>=0.11.1',
    'platformio>=3.5.3',
    'pyyaml>=3.12',
    'paho-mqtt>=1.3.1',
    'colorlog>=3.1.2',
    'tornado>=5.0.0',
    'esptool>=2.3.1',
]

CLASSIFIERS = [
    'Environment :: Console',
    'Intended Audience :: Developers',
    'Intended Audience :: End Users/Desktop',
    'License :: OSI Approved :: MIT License',
    'Programming Language :: C++',
    'Programming Language :: Python :: 2 :: Only',
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
    python_requires='>=2.7,<3',
    install_requires=REQUIRES,
    keywords=['home', 'automation'],
    entry_points={
        'console_scripts': [
            'esphomeyaml = esphomeyaml.__main__:main'
        ]
    },
    packages=find_packages()
)
