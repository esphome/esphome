from typing import Union, Dict, Any, Optional

# Typings
TreeItem = Union[dict, list, str]
PackageConfig = Union[dict, str]
PackageSource = Union[dict, str]
PackageParams = Dict[str, Any]


class PackageDefinition(object):
    """
    The class representing package instantiated with particular params
    """

    def __init__(self, local_name: str, parent: Optional['PackageDefinition'] = None) -> None:
        self.local_name: str = local_name
        self.parent: Optional[PackageDefinition] = parent
        self.content: Optional[dict] = None
        self.external_ref: Optional[str] = None
        self.locator: Optional[ParsedLocator] = None
        self.file_system_location: Optional[str] = None
        self.params: PackageParams = {}


class ParsedLocator(object):
    DEFAULT_PACKAGE_FILE = 'main.yaml'
    FILE_LOCATOR_SUFFIXES = ['.yaml', '.yml']

    PATH_SEPARATOR = '/'

    def __init__(self, locator: Optional[str]) -> None:
        self.original_locator = locator
        self.type: str = None
        self.repository: str = None
        self.path: str = None
        self.tag: str = None
        if locator is not None:
            self.parse(locator)

    def _fetch_tag(self):
        # Separate tag part if any
        if '@' in self.path:
            tag_split = self.path.split('@')
            if len(tag_split) > 0:
                self.tag = tag_split[-1]
                self.path = '@'.join(tag_split[:1])

    def parse(self, locator: str):
        # Separate type part from the rest of the string
        # The rest is considered as a path, repository should be parsed by subclass if relevant
        self.type, self.path = locator.split(':', 1)

    def is_file_locator(self) -> bool:
        """
        Returns true if locator points the yaml file and false if it
        points a folder
        """
        if self.path is None:
            return False
        for suffix in ParsedLocator.FILE_LOCATOR_SUFFIXES:
            if self.path.endswith(suffix):
                return True
        return False

    def get_path_to_file(self) -> Optional[str]:
        if self.path is None:
            return None
        return self.path if self.is_file_locator() else self.PATH_SEPARATOR.join(
            (self.path, self.DEFAULT_PACKAGE_FILE)
        )
