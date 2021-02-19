import abc

from .common import PackageDefinition


class BaseSourceLoader(object, metaclass=abc.ABCMeta):

    @abc.abstractmethod
    def load(self, locator: str, package: PackageDefinition):
        raise NotImplementedError


class LocalSourceLoader(BaseSourceLoader):

    def load(self, locator: str, package: PackageDefinition):
        pass

