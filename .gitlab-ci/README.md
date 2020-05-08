Evince CI infrastructure
------------------------

Evince uses Docker containers, generated using `docker`. Evince uses
a custom image to install large number of packages, like the LaTeX ones,
and ghostscript, which are are required to build support for DVI and
PostScript.

Building a Docker image for Evince CI
-------------------------------------

Steps for building a docker image:

```
docker login registry.gitlab.gnome.org
```

```
$ docker build -t registry.gitlab.gnome.org/gnome/evince/master-amd64 -f ubuntu.Dockerfile .
$ docker push registry.gitlab.gnome.org/gnome/evince/master-amd64
```
