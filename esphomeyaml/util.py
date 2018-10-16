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
