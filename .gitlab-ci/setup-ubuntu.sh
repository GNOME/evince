#!/bin/bash

export DEBIAN_FRONTEND=noninteractive

apt-get -yqq update
apt-get -yqq install apt-utils
apt-get -yq install gnome-common libglib2.0-dev-bin \
                    yelp-tools itstool gtk-doc-tools \
					appstream \
				    libgirepository1.0-dev  \
                    libgtk-3-dev libgstreamer1.0-dev \
				    libgstreamer-plugins-base1.0-dev \
				    libxml2-dev libxml2-utils \
				    libnautilus-extension-dev \
				    libsecret-1-dev libgspell-1-dev libgnome-desktop-3-dev \
                    libpoppler-glib-dev poppler-data \
				    libspectre-dev libtiff5-dev libdjvulibre-dev \
				    libkpathsea-dev libarchive-dev libgxps-dev \
				ccache systemd ninja-build python3-pip
apt-get clean
rm -rf /var/lib/apt/lists/*

pip3 install meson

useradd -u 1984 -ms /bin/bash user
