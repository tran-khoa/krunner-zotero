#!/bin/bash

set -e

krunner_version=$(krunner --version | grep -oP "(?<=krunner )\d+")
cd build
xargs sudo rm < install_manifest.txt

# KRunner needs to be restarted for the changes to be applied
if pgrep -x krunner > /dev/null
then
    kquitapp$krunner_version krunner
fi

echo "Plugin uninstalled"