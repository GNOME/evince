#!/usr/bin/env bash -e

scriptdir=`dirname $0`
olddir=`pwd`
cd $scriptdir

rm -f org.gnome.Evince.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

flatpak-builder --ccache --force-clean _build org.gnome.Evince.json --repo=_repo
flatpak build-bundle _repo org.gnome.Evince.flatpak org.gnome.Evince master

cd $olddir
