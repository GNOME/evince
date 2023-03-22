FROM ubuntu:22.04

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
    cmake desktop-file-utils \
&& apt-get -yqq install --no-install-recommends \
    python3-pip python3-jinja2 python3-toml python3-typogrify \
&& pip install gi-docgen \
&& apt-get -yqq install --no-install-recommends \
    poppler-data libboost-container-dev libopenjp2-7-dev libcurl4-openssl-dev \
&& git clone --depth 1 --branch poppler-23.02.0 \
    https://gitlab.freedesktop.org/poppler/poppler.git /tmp/poppler \
&& cd /tmp/poppler \
&& cmake -DBUILD_GTK_TESTS=OFF, -DBUILD_CPP_TESTS=OFF, -DENABLE_UTILS=OFF, \
    -DENABLE_CPP=OFF, -DENABLE_GOBJECT_INTROSPECTION=OFF, \
    -DENABLE_LIBOPENJPEG=openjpeg2 -DENABLE_QT5=OFF -DENABLE_QT6=OFF \
    -DBUILD_GTK_TESTS=OFF -DBUILD_CPP_TESTS=OFF -G Ninja . \
&& ninja && ninja install \
&& apt-get clean \
&& rm -rf /tmp/poppler \
&& useradd -u 1984 -ms /bin/sh user

USER user
WORKDIR /home/user
