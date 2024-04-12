#!/usr/bin/with-contenv bashio
# ==============================================================================
# This files installs the user ESPHome fork if specified
# The fork must be up to date with the latest ESPHome dev branch
# and have no conflicts.
# This config option only exists ont eh ESPHome Dev add-on.
# ==============================================================================

declare esphome_fork

if bashio::config.has_value 'esphome_fork'; then
  esphome_fork=$(bashio::config 'esphome_fork')
  if [[ $esphome_fork == *":"* ]]; then
    IFS=':' read -r -a array <<<"$esphome_fork"
    username=${array[0]}
    ref=${array[1]}
  else
    username="esphome"
    ref=$esphome_fork
  fi
  full_url="https://github.com/${username}/esphome/archive/${ref}.tar.gz"
  bashio::log.info "Checking forked ESPHome"
  dev_version=$(python3 -c "from esphome.const import __version__; print(__version__)")
  bashio::log.info "Downloading ESPHome from fork '${esphome_fork}' (${full_url})..."
  curl -L -o /tmp/esphome.tar.gz "${full_url}" -qq ||
    bashio::exit.nok "Failed downloading ESPHome fork."
  bashio::log.info "Installing ESPHome from fork '${esphome_fork}' (${full_url})..."
  rm -rf /esphome || bashio::exit.nok "Failed to remove ESPHome."
  mkdir /esphome
  tar -zxf /tmp/esphome.tar.gz -C /esphome --strip-components=1 ||
    bashio::exit.nok "Failed installing ESPHome from fork."
  pip install -U -e /esphome || bashio::exit.nok "Failed installing ESPHome from fork."
  rm -f /tmp/esphome.tar.gz
  fork_version=$(python3 -c "from esphome.const import __version__; print(__version__)")

  if [[ "$fork_version" != "$dev_version" ]]; then
    bashio::log.error "############################"
    bashio::log.error "Uninstalled fork as version does not match"
    bashio::log.error "Update (or ask the author to update) the branch"
    bashio::log.error "This is important as the dev addon and the dev ESPHome"
    bashio::log.error "branch can have changes that are not compatible with old forks"
    bashio::log.error "and get reported as bugs which we cannot solve easily."
    bashio::log.error "############################"
    bashio::exit.nok
  fi
  bashio::log.info "Installed ESPHome from fork '${esphome_fork}' (${full_url})..."
fi
