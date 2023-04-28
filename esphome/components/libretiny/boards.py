import requests


def fetch_board_list() -> dict:
    url = "https://docs.libretiny.eu/boards.json"
    boards = {}
    with requests.get(url, timeout=2.0) as r:
        boards_api = r.json()

    for board in boards_api:
        name = board["name"]
        title = board["title"]
        vendor = board["vendor"]

        if vendor not in boards:
            boards[vendor] = {
                "title": vendor,
                "items": {},
            }
        boards[vendor]["items"][name] = title

    return list(boards.values())
