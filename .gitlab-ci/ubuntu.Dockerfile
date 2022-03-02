FROM ubuntu:21.10

ENV LANG C.UTF-8

ADD setup-ubuntu.sh /opt/
RUN /bin/sh /opt/setup-ubuntu.sh

USER user
WORKDIR /home/user
