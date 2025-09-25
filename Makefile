.PHONY: clean live-html check-links anchors production convert-from-rst convert-branch-in-place directories netlify repo-data all

SHELL := bash
.SHELLFLAGS := -euo pipefail -c


PAGEFIND=$(shell command -v pagefind >/dev/null 2>&1 && echo "pagefind" || echo "npx --yes pagefind@1.3.0")

export HUGO_PARAMS_COMMIT_HASH=$(shell git rev-parse --short HEAD)
export HUGO_PARAMS_COMMIT_TITLE=$(shell git log -1 --pretty=%s)
export HUGO_PARAMS_BRANCH=$(shell git branch --show-current)
export HUGO_PARAMS_REPO_URL=$(shell git config --get remote.origin.url)

all: production

production: anchors
	hugo --minify
	$(PAGEFIND)
	hugo --minify

directories:
	mkdir -p data public pagefind content static

check-links: anchors
	hugo --environment production

anchors: repo-data
	$(PAGEFIND) -s pagefind-bootstrap
	hugo --environment anchors
	python3 script/md_anchors.py

repo-data: directories
	mkdir -p data/automations
	curl -s -S https://data.esphome.io/release/automations.json | script/collate_automations.sh > data/automations/current.json
	curl -s -S https://data.esphome.io/beta/automations.json | script/collate_automations.sh > data/automations/beta.json
	curl -s -S https://data.esphome.io/dev/automations.json | script/collate_automations.sh > data/automations/next.json

live-html:	anchors
	$(PAGEFIND)
	hugo server --bind 0.0.0.0 --port 8000 --baseURL http://localhost:8000

clean:
	rm -rf public/*
	rm -rf pagefind/*
	rm -rf data/automations/
	rm -rf data/repo.yaml
	rm -rf resources/_gen
	hugo mod clean

convert-branch-in-place:
	sh script/migrate.sh


netlify: repo-data
	$(PAGEFIND) -s pagefind-bootstrap
	hugo --environment anchors
	python3 script/md_anchors.py
	hugo --minify
	$(PAGEFIND)
	# rerun hugo to incorporate generated index
	hugo --minify
