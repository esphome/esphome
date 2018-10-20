from __future__ import print_function


class Registry(dict):
    def register(self, name):
        def decorator(fun):
            self[name] = fun
            return fun

        return decorator


class ServiceRegistry(dict):
    def register(self, name, validator):
        def decorator(fun):
            self[name] = (validator, fun)
            return fun

        return decorator


def safe_print(message=""):
    try:
        print(message)
        return
    except UnicodeEncodeError:
        pass

    try:
        print(message.encode('ascii', 'backslashreplace'))
    except UnicodeEncodeError:
        print("Cannot print line because of invalid locale!")
