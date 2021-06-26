FROM ubuntu:20.10

ENV LANG C.UTF-8

ADD setup-ubuntu.sh /opt/
RUN /bin/bash /opt/setup-ubuntu.sh

USER user
WORKDIR /home/user
