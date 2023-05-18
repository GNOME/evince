Title: Profiling

Evince makes use of [Sysprof](https://developer.gnome.org/documentation/tools/sysprof.html) for profiling. To do profiling on Evince, it is first necessary to build evince in debug mode: `meson setup -Ddebug=true`, and to have the latest developer headers for `sysprof` installed.

## Profiling in action

Once compiled having Sysprof available, Evince can be started through Sysprof. The most relevant information profiled to date is related to `EvJobs`, that will be available under `Timings`.
