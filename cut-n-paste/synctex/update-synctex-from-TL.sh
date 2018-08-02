#!/bin/sh
# update-synctex-from-TL.sh
#
# Get latest synctex parser from TexLive SVN repository.
SVN_URI=svn://tug.org/texlive/trunk/Build/source/texk/web2c/synctexdir
SCRIPT_NAME=update-synctex-from-TL.sh
FILES="synctex_parser_version.txt synctex_parser.c  synctex_parser.h  synctex_parser_advanced.h synctex_parser_local.h  synctex_parser_utils.c  synctex_parser_utils.h synctex_version.h"

  echo "Obtaining latest version of the sources"
  for FILE in $FILES 
  do
    svn export $SVN_URI/$FILE
  done

