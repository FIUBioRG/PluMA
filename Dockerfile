# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest

ENV CC="clang"
ENV CXX="clang++"
ENV PLUMA_DIRECTORY="/app"

WORKDIR /app

ADD ./ ./

RUN apk add --no-cache build-base scons blas clang-dev musl-dev blas-dev perl-dev git R-dev R R-mathlib python3-dev libffi-dev libexecinfo-dev libc-dev py3-pip py3-pandas py3-numpy swig pcre-dev chrony \
  && ln -sf /usr/bin/python3 /usr/bin/python \
  && ln -sf /usr/bin/pip3 /usr/bin/pip \
  && mkdir -p /root/.R \
  && printf 'CXX=clang++\nPKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')" \
  && pip install pythonds \
  && scons \
  && rm -rf /var/cache/apk/*

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]

CMD ["/app/pluma"]
