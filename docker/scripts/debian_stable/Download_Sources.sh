#!/bin/bash

set -eu

mkdir -pv all_sources && cd all_sources

# Ensure apt has a valid deb-src URI.
#
# NOTE: This will probbaly fail if atypical repositories are being used.
if [ -z "$(grep '^deb-src' /etc/apt/sources.list)" ] ; then
    sed -i -e 's/^deb \(.*\)/deb \1\ndeb-src \1/' /etc/apt/sources.list
fi

# Have to refresh package DB for apt to honour it.
export DEBIAN_FRONTEND="noninteractive"
apt-get update --yes </dev/null


# Download source for every installed package (which is available).
#
# Note: the following was motivated by https://askubuntu.com/a/1040020
#dpkg-query -f '${source:Package}=${Version}\n' -W |
dpkg-query -f '${source:Package}\n' -W |
  sort -u |
  while read apkg ; do
    ( # Determine the currently installed version number.
      #
      # Note: seems to fail most of the time.
      #version="$( apt policy "${apkg}" 2>/dev/null |
      #              grep 'Installed: ' |
      #              sed -e 's/.*Installed: //' )"
      #pkg_v="${apkg}=${version}"

      mkdir -pv "${apkg}" && cd "${apkg}"

      apt-get --quiet --quiet --download-only source "${apkg}" || true

    ) </dev/null 
done

rmdir ./*/ &>/dev/null

# Copy the custom package repos.
( mkdir -pv wt && cd wt
  tar --gz -cf wt.tgz /wt
)
( mkdir -pv ygor && cd ygor
  tar --gz -cf ygor.tgz /ygor
)
( mkdir -pv ygorclustering && cd ygorclustering
  tar --gz -cf ygorclustering.tgz /ygorcluster
)
( mkdir -pv explicator && cd explicator
  tar --gz -cf explicator.tgz /explicator
)
( mkdir -pv dicomautomaton && cd dicomautomaton
  tar --gz -cf dicomautomaton.tgz /dcma
)

printf 'Done.\n'

