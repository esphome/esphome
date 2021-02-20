import os

from esphome.core import CORE


def get_abs_path_from_config_relative(path: str) -> str:
    """
    Returns absolute path for given root config relative path
    :param path: config relative path
    """
    path = os.path.expanduser(path)
    return os.path.abspath(os.path.join(CORE.config_dir, path))


def get_abs_path_from_package_relative(path: str, package):
    """
    Returns absolute path for given package-relative path
    :param path: package-relative path
    :type package: PackageDefinition
    """
    path = os.path.expanduser(path)
    if package.file_system_location is None:
        raise ValueError('Can\'t resolve package-relative path for package without source locator')
    return os.path.abspath(os.path.join(os.path.dirname(package.file_system_location), path))
