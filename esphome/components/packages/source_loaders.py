import abc
import os.path

import esphome.config_validation as cv
from esphome import yaml_util
from .utils import get_abs_path_from_config_relative, get_abs_path_from_package_relative
from .common import PackageDefinition, ParsedLocator

DEFAULT_PACKAGE_FILE = 'main.yaml'
FILE_LOCATOR_SUFFIXES = ['.yaml', '.yml']


class BaseSourceLoader(object, metaclass=abc.ABCMeta):

    @abc.abstractmethod
    def load(self, locator: str, package: PackageDefinition):
        raise NotImplementedError


class LocalLocator(ParsedLocator):
    PATH_SEPARATOR = os.path.sep


class LocalSourceLoader(BaseSourceLoader):

    def load(self, locator: str, package: PackageDefinition):
        package.locator = LocalLocator(locator)
        file_path = package.locator.get_path_to_file()
        # All non-absolute path should be treated as relative to the file the package
        # was referenced from
        if not os.path.isabs(file_path):
            # for root config - it is relative to config location
            if package.parent is None:
                file_path = get_abs_path_from_config_relative(file_path)
            else:
                file_path = get_abs_path_from_package_relative(file_path, package.parent)
        if not os.path.exists(file_path):
            raise cv.Invalid("Invalid package declaration. "
                             "File {} doesn't exist".format(file_path))
        package.file_system_location = file_path
        package.content = yaml_util.load_yaml_package(package.file_system_location)
