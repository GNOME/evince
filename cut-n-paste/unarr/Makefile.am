# This Makefile refers to sources in subdirectories.  Option "subdir-objects"
# tells Automake to put the compiled objects in the same directory as the
# source file.  Without this option Automake prints warnings related to
# forward compatibility.
AUTOMAKE_OPTIONS = subdir-objects

# Include subdirs to generate .gitignore files.
SUBDIRS = common lzmasdk rar

noinst_LTLIBRARIES = libunarr.la

libunarr_la_SOURCES =		\
	unarr.h			\
	rar/filter-rar.c	\
	rar/lzss.h		\
	rar/rar.h		\
	rar/rarvm.h		\
	rar/huffman-rar.c	\
	rar/uncompress-rar.c	\
	rar/rar.c		\
	rar/rarvm.c		\
	rar/parse-rar.c		\
	common/unarr-imp.h	\
	common/allocator.h	\
	common/unarr.c		\
	common/stream.c		\
	common/conv.c		\
	common/crc32.c		\
	lzmasdk/Precomp.h	\
	lzmasdk/7zTypes.h	\
	lzmasdk/Ppmd7.h		\
	lzmasdk/Ppmd7Dec.c	\
	lzmasdk/CpuArch.h	\
	lzmasdk/CpuArch.c	\
	lzmasdk/Ppmd7.c		\
	lzmasdk/Ppmd.h

libunarr_la_CPPFLAGS = -DHAVE_ZLIB
libunarr_la_LIBADD = $(ZLIB_LIBS)

EXTRA_DIST = COPYING AUTHORS

-include $(top_srcdir)/git.mk
