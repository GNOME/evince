#!/bin/bash -e

scriptdir=`dirname $0`
olddir=`pwd`
cd $scriptdir

rm -f org.gnome.evince.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

flatpak-builder --ccache --force-clean _build org.gnome.Evince.json --repo=_repo
flatpak build-bundle _repo org.gnome.evince.flatpak org.gnome.evince master

cd $olddir
