from typing import Union, Dict, Any

# Typings
TreeItem = Union[dict, list, str]
PackageConfig = Union[dict, str]
PackageSource = Union[dict, str]
PackageParams = Dict[str, Any]


class PackageDefinition(object):
    """
    The class representing package instantiated with particular params
    """

    def __init__(self, local_name: str) -> None:
        self.local_name: str = local_name
        self.content: dict = None
        self.external_ref: str = None
        self.params: PackageParams = {}
