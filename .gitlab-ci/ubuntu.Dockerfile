FROM ubuntu:23.10

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
    libexempi-dev \
    libarchive-dev \
    libsecret-1-dev libgspell-1-dev \
    libgnome-desktop-3-dev libnautilus-extension-dev \
    libspectre-dev libtiff5-dev libdjvulibre-dev \
    libkpathsea-dev libgxps-dev libsynctex-dev \
    libgtk-4-dev libgnome-desktop-4-dev libadwaita-1-dev \
    git ccache systemd ninja-build meson \
    cmake desktop-file-utils gi-docgen \
&& apt-get -yqq install --no-install-recommends \
    poppler-data libboost-container-dev libopenjp2-7-dev libcurl4-openssl-dev \
&& git clone --depth 1 --branch poppler-23.10.0 \
    https://gitlab.freedesktop.org/poppler/poppler.git /tmp/poppler \
&& cd /tmp/poppler \
&& cmake -DBUILD_GTK_TESTS=OFF -DBUILD_CPP_TESTS=OFF \
    -DBUILD_MANUAL_TESTS=OFF -DENABLE_CPP=OFF \
    -DENABLE_GOBJECT_INTROSPECTION=OFF -DENABLE_LIBOPENJPEG=openjpeg2 \
    -DENABLE_QT5=OFF -DENABLE_QT6=OFF \
    -DENABLE_UTILS=OFF -DENABLE_NSS3=OFF -DENABLE_GPGME=OFF \
    -DENABLE_LCMS=OFF -DENABLE_LIBCURL=OFF -G Ninja . \
&& ninja && ninja install \
&& apt-get clean \
&& rm -rf /tmp/poppler \
&& useradd -u 1984 -ms /bin/sh user

USER user
WORKDIR /home/user
