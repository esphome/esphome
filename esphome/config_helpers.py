from esphome.const import CONF_ID


class Extend:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"!extend {self.value}"

    def __repr__(self):
        return f"Extend({self.value})"

    def __eq__(self, b):
        """
        Check if two Extend objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Extend) and self.value == b.value


class Remove:
    def __init__(self, value=None):
        self.value = value

    def __str__(self):
        return f"!remove {self.value}"

    def __repr__(self):
        return f"Remove({self.value})"

    def __eq__(self, b):
        """
        Check if two Remove objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Remove) and self.value == b.value


def merge_config(full_old, full_new):
    def merge(old, new):
        if isinstance(new, dict):
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                if isinstance(v, Remove) and k in old:
                    del res[k]
                else:
                    res[k] = merge(old[k], v) if k in old else v
            return res
        if isinstance(new, list):
            if not isinstance(old, list):
                return new
            res = old.copy()
            ids = {
                v_id: i
                for i, v in enumerate(res)
                if (v_id := v.get(CONF_ID)) and isinstance(v_id, str)
            }
            extend_ids = {
                v_id.value: i
                for i, v in enumerate(res)
                if (v_id := v.get(CONF_ID)) and isinstance(v_id, Extend)
            }

            ids_to_delete = []
            for v in new:
                if new_id := v.get(CONF_ID):
                    if isinstance(new_id, Extend):
                        new_id = new_id.value
                        if new_id in ids:
                            v[CONF_ID] = new_id
                            res[ids[new_id]] = merge(res[ids[new_id]], v)
                            continue
                    elif isinstance(new_id, Remove):
                        new_id = new_id.value
                        if new_id in ids:
                            ids_to_delete.append(ids[new_id])
                            continue
                    elif (
                        new_id in extend_ids
                    ):  # When a package is extending a non-packaged item
                        extend_res = res[extend_ids[new_id]]
                        extend_res[CONF_ID] = new_id
                        new_v = merge(v, extend_res)
                        res[extend_ids[new_id]] = new_v
                        continue
                    else:
                        ids[new_id] = len(res)
                res.append(v)
            res = [v for i, v in enumerate(res) if i not in ids_to_delete]
            return res
        if new is None:
            return old

        return new

    return merge(full_old, full_new)
