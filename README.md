# ![evince-logo] Evince

Evince is a document viewer capable of displaying multiple and single
page document formats like PDF and Postscript.  For more general
information about Evince please visit our website at
https://wiki.gnome.org/Apps/Evince.

This software is licensed under the [GPLv2][license].

[![flatpak]](https://flathub.org/apps/details/org.gnome.Evince)

## Evince Requirements

* [GNOME Platform libraries][gnome]
* [Poppler for PDF viewing][poppler]

## Evince Optional Backend Libraries

* [Spectre for PostScript (PS) viewing][ghostscript]
* [DjVuLibre for DjVu viewing][djvulibre]
* [Kpathsea for Device-independent file format (DVI) viewing][dvi]
* [Archive library for Comic Book Resources (CBR) viewing][comics]
* [LibTiff for Multipage TIFF viewing][tiff]
* [LibGXPS for XML Paper Specification (XPS) viewing][xps]

## Default branch renamed to `main`

The default development branch of Evince has been renamed to `main`. To update
your local checkout, use:
```sh
git checkout master
git branch -m master main
git fetch
git branch --unset-upstream
git branch -u origin/main
git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main
```

[gnome]: https://www.gnome.org/
[poppler]: https://poppler.freedesktop.org/
[ghostscript]: https://www.freedesktop.org/wiki/Software/libspectre/
[djvulibre]: https://djvulibre.djvuzone.org/
[dvi]: https://tug.org/texinfohtml/kpathsea.html
[comics]: https://libarchive.org/
[tiff]: http://libtiff.org/
[xps]: https://wiki.gnome.org/Projects/libgxps
[license]: COPYING
[evince-logo]: data/icons/scalable/apps/org.gnome.Evince.svg
[flatpak]: https://flathub.org/api/badge?svg&locale=en
