#!/bin/sh

# Copyright (C) 2019 Red Hat, Inc.
# Author: Bastien Nocera <hadess@hadess.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.


# Add to your top-level meson.build to check for an updated NEWS file
# when doing a "dist" release, similarly to automake's check-news:
# https://www.gnu.org/software/automake/manual/html_node/List-of-Automake-options.html
#
# Checks NEWS for the version number:
# meson.add_dist_script(
#   find_program('check-news.sh').path(),
#   '@0@'.format(meson.project_version())
# )
#
# Checks NEWS and data/foo.appdata.xml for the version number:
# meson.add_dist_script(
#   find_program('check-news.sh').path(),
#   '@0@'.format(meson.project_version()),
#   'NEWS',
#   'data/foo.appdata.xml'
# )

usage()
{
	echo "$0 VERSION [FILES...]"
	exit 1
}

check_version()
{
	VERSION=$1
	# Look in the first 15 lines for NEWS files, but look
	# everywhere for other types of files
	if [ "$2" = "NEWS" ]; then
		DATA=`sed 15q $SRC_ROOT/"$2"`
	else
		DATA=`cat $SRC_ROOT/"$2"`
	fi
	case "$DATA" in
	*"$VERSION"*)
		:
		;;
	*)
		echo "$2 not updated; not releasing" 1>&2;
		exit 1
		;;
	esac
}

has_verify_latest_version()
{
	appstream-util 2>&1 | grep -q get-latest-version
	return $?
}

verify_latest_version()
{
	VERSION=$1
	FILE=$SRC_ROOT/"$2"
	LATEST_VERSION=`appstream-util get-latest-version "$FILE" | tr -d '\d'`
	if [ "$LATEST_VERSION" != "$VERSION" ] ; then
		echo "Expected latest version $VERSION in $FILE, got $LATEST_VERSION"
		exit 1
	fi
}

SRC_ROOT=${MESON_DIST_ROOT:-"./"}

if [ $# -lt 1 ] ; then usage ; fi

VERSION=$1
shift

if [ $# -eq 0 ] ; then
	check_version $VERSION 'NEWS'
	exit 0
fi

for i in $@ ; do
	case "$i" in
	*"metainfo"*)
		if has_verify_latest_version ; then
			verify_latest_version $VERSION "$i"
		fi
		;;
	esac

	check_version $VERSION "$i"
done

exit 0
