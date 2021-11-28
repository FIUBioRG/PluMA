# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM ubuntu:latest

WORKDIR /app

COPY ./ ./

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/R/site-library/RInside/lib/

RUN rm -f /etc/apt/apt.conf.d/docker-clean; echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

RUN --mount=type=cache,target=/var/cache/apt --mount=type=cache,target=/var/lib/apt --mount=type=cache,target=/usr/lib/R/site-library/ \
  apt-get update -qq \
  && apt-get install -y software-properties-common dirmngr wget \
  && wget -qO- https://cloud.r-project.org/bin/linux/ubuntu/marutter_pubkey.asc | tee -a /etc/apt/trusted.gpg.d/cran_ubuntu_key.asc \
  && add-apt-repository "deb https://cloud.r-project.org/bin/linux/ubuntu $(lsb_release -cs)-cran40/" \
  && apt-get update -qq \
  && apt-get install -y build-essential \
    gcc g++ scons perl libperl-dev libffi-dev libc++-11-dev \
    pkg-config python3 python3-dev \
    python3-pip libpcre++-dev r-base git swig libpcre3-dev libpcre3 libpcre++-dev openjdk-8-jdk-headless \
  && pip install pythonds numpy scipy pandas \
  && Rscript -e "if(!require(RInside)) install.packages('RInside', repos = 'https://cloud.r-project.org')" \
  && echo "/usr/local/lib/R/site-library/RInside/lib" > /etc/ld.so.conf.d/RInside.conf \
  && scons \
  && apt-get clean \
  && ldconfig

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]
VOLUME ["/app/logs"]
VOLUME ["/app/config"]

ENTRYPOINT ["/app/docker-entrypoint.sh"]
