FROM ubuntu:26.04

ENV LANG=C.UTF-8 DEBIAN_FRONTEND=noninteractive

RUN apt-get -yqq update \
&& apt-get -yqq install --no-install-recommends \
    apt-utils \
&& apt-get -yqq install --no-install-recommends \
    gnome-common libglib2.0-dev-bin \
    yelp-tools itstool appstream \
    libgirepository1.0-dev libgtk-3-dev \
    libhandy-1-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libxml2-dev libxml2-utils \
    libarchive-dev \
    libsecret-1-dev libgspell-1-dev \
    libgnome-desktop-3-dev libnautilus-extension-dev \
    libspectre-dev libtiff5-dev libdjvulibre-dev \
    libkpathsea-dev libgxps-dev libsynctex-dev \
    git ccache systemd ninja-build meson \
    cmake desktop-file-utils gi-docgen \
&& apt-get -yqq install --no-install-recommends \
    poppler-data libpoppler-dev libpoppler-glib-dev \
&& apt-get clean \
&& useradd -u 1984 -ms /bin/sh user

USER user
WORKDIR /home/user
