$!========================================================================
$!
$! Goo library compile script for VMS.
$!
$! Copyright 1996 Derek B. Noonburg
$!
$!========================================================================
$!
$ GOO_OBJS = "GString.obj,gmempp.obj,gfile.obj,gmem.obj,parseargs.obj" + -
             ",vms_directory.obj,vms_unix_times.obj"
$ if f$extract(1,3,f$getsyi("Version")) .lts. "7.0"
$  then
$   GOO_OBJS = GOO_OBJS + ",vms_unlink.obj"
$   CCOMP vms_unlink.c
$ endif
$!
$ CXXCOMP GString.cc
$ CXXCOMP gmempp.cc
$ CXXCOMP gfile.cc
$ CCOMP gmem.c
$ CCOMP parseargs.c
$ CCOMP vms_directory.c
$ CCOMP vms_unix_times.c
$!
$ lib/cre libgoo.olb
$ lib libgoo 'GOO_OBJS
