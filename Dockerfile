# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM ubuntu:latest

WORKDIR /app

COPY ./ ./

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/R/site-library/RInside/lib/

ENV PERL_MM_USE_DEFAULT=1
ENV JAVA_HOME="$(readlink -f /usr/bin/java | sed 's/\/bin\/java//')"

ARG DEBIAN_FRONTEND=noninteractive

ADD https://cloud.r-project.org/bin/linux/ubuntu/marutter_pubkey.asc /etc/apt/trusted.gpg.d/cran_ubuntu_key.asc

RUN rm -f /etc/apt/apt.conf.d/docker-clean; echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

RUN --mount=type=cache,target=/var/cache/apt --mount=type=cache,target=/var/lib/apt --mount=type=cache,target=/usr/lib/R/site-library \
  --mount=type=cache,target=/usr/lib/python3/dist-packages \
  chmod 755 /etc/apt/trusted.gpg.d/cran_ubuntu_key.asc \
  && apt-get update -qq \
  && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    gcc \
    git \
    libc++-11-dev \
    libpcre++-dev \
    libpcre3 \
    libpcre3-dev \
    libperl-dev \
    openjdk-8-jdk-headless \
    perl \
    perl-doc \
    pkg-config \
    python3 \
    python3-dev \
    python3-pip \
    scons \
    software-properties-common \
    swig \
  && add-apt-repository "deb https://cloud.r-project.org/bin/linux/ubuntu $(lsb_release -cs)-cran40/" \
  && apt-get update -qq \
  && apt-get install -y --no-install-recommends r-base \
  && pip3 install pythonds numpy scipy pandas \
  && Rscript -e "if(!require(RInside)) install.packages('RInside', repos = 'https://cloud.r-project.org')" \
  && echo "/usr/local/lib/R/site-library/RInside/lib" > /etc/ld.so.conf.d/RInside.conf \
  && scons \
  && apt-get clean \
  && ldconfig \
  && update-java-alternatives --auto

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]
VOLUME ["/app/logs"]
VOLUME ["/app/config"]

ENTRYPOINT ["/app/docker-entrypoint.sh"]
