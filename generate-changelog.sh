#!/bin/sh

if [ -d "$1/.git" ]; then
	git log -M -C --name-status --date=short --no-color | fmt --split-only > $2.tmp &&
	mv -f $2.tmp $2 ||
	{
		rm -f $2.tmp &&
		echo "Failed to generate ChangeLog, your ChangeLog may be outdated" >&2;
	}
else
	echo "Git repository not found" >&2
	exit 1
fi
