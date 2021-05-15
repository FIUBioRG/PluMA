# Copyright 2019-2020 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest

ENV CXX="clang++"
ENV PLUMA_DIRECTORY="/pluma"

WORKDIR /pluma

ADD ./ ./
ADD https://github.com/just-containers/s6-overlay/releases/latest/download/s6-overlay-amd64.tar.gz ./

RUN tar xf s6-overlay-amd64.tar.gz -C / \
&& rm s6-overlay-amd64.tar.gz

RUN apk add -t .runtime-deps \
  blas \
  clang \
  gcc \
  musl \
  perl \
  git \
  R \
  R-mathlib \
  python3 \
  libffi \
  libc-dev \
  musl-dev \
&& apk add -t .build-deps \
  blas-dev \
  perl-dev \
  R-dev \
  python3-dev \
  libffi-dev \
  build-base \
  libexecinfo-dev \
&& ln -s /usr/bin/python3 /usr/bin/python \
&& ln -s /usr/bin/pip3 /usr/bin/pip \
&& ln -s /usr/bin/python3-config /usr/bin/python-config \
&& echo $PLUMA_DIRECTORY >> /etc/environment \
&& mkdir ~/.R \
&& printf 'CXX=clang++\nPKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
&& Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')" \
&& scons -Q \
&& apk del .build-deps

VOLUME ["/pluma/plugins"]
VOLUME ["/pluma/pipelines"]

ENTRYPOINT ["/pluma/pluma"]
CMD ["/init"]
