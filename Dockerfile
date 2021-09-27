# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM ubuntu:latest

WORKDIR /app

COPY ./ ./

RUN apt update -qq \
  && apt install --no-install-recommends -y software-properties-common dirmngr wget \
  && wget -qO- https://cloud.r-project.org/bin/linux/ubuntu/marutter_pubkey.asc | tee -a /etc/apt/trusted.gpg.d/cran_ubuntu_key.asc \
  && add-apt-repository "deb https://cloud.r-project.org/bin/linux/ubuntu $(lsb_release -cs)-cran40/" \
  && apt update -qq \
  && apt install -y --no-install-recommends build-essential \
    gcc g++ scons perl libperl-dev libffi-dev libc++-11-dev \
    pkg-config python3 python3-dev python3-numpy python3-pandas \
    python3-pip libpcre++-dev r-base git swig \
  && pip install pythonds \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')" \
  && scons \
  && chmod +x pluma \
  && apt-get clean

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]

CMD ["/app/pluma"]
ENTRYPOINT ["/bin/sh"]
