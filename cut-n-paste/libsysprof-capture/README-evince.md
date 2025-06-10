# Sysprof in Evince

This directory contains a copy of libsysprof-capture from the
official Sysprof repository.

  - URL: https://gitlab.gnome.org/GNOME/sysprof
  - Commit: b6e3a34a93c05b616e7c157fc9a701d431c52966

The simplest way of updating its contents is to re-create the directory
from scratch, and restoring the needed files with Git. Example commands:

```sh
  rm -rf cut-n-paste/libsysprof-capture
  cp -ar ../path/to/sysprof/src/libsysprof-capture cut-n-paste/
  git checkout -- \
      cut-n-paste/libsysprof-capture/README-evince.md \
      cut-n-paste/libsysprof-capture/meson.build \
      cut-n-paste/libsysprof-capture/include/config.h
  git rm -r cut-n-paste/libsysprof-capture/tests
  git add -A cut-n-paste/libsysprof-capture
```

It can work with earlier versions after applying the following patch the make
the compiler happy when using `-Werror=missing-prototypes`, which is used when
building Evince with `ev_debug` enabled.

```diff
--- sysprof-capture-util.c.orig	2025-06-10 16:42:21.796525223 -0400
+++ sysprof-capture-util.c	2025-06-10 16:41:13.189360028 -0400
@@ -74,6 +74,7 @@
 static void *_sysprof_io_sync_lock = SRWLOCK_INIT;
 #endif
 
+#ifndef __linux__
 size_t
 (_sysprof_getpagesize) (void)
 {
@@ -234,6 +235,7 @@
   errno = 0;
   return total;
 }
+#endif /* #ifndef __linux__ */
 
 size_t
 (_sysprof_strlcpy) (char       *dest,

```

The sysprof-version.h file has to be generated to be included in this
directory, instead of including only the .h.in template. To obtain it,
run the following at the Sysprof source tree:

```sh
  meson setup build \
  	-Dhelp=false -Dsysprofd=none -Dlibsysprof=false \
	-Dtools=false -Dtests=false -Dexamples=false -Dgtk=false
  cp build/src/libsysprof-capture/sysprof-version.h \
    ../path/to/evince/cut-n-paste/libsysprof-capture/
```

Then check whether the updated sources require any changes to the meson
build system, and update the Sysprof commit identifier and tag in this
file.
