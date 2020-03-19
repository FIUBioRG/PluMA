# Copyright 2019 Florida International University
# SPDX-License-Identifier: MIT

FROM archlinux:latest AS builder

ENV CXX="clang++"
# ARG PYPY_VERSION=7.3.0
# ENV PYPY_VERSION ${PYPY_VERSION}

WORKDIR /tmp/source

ADD ./ ./

RUN pacman -Sy --noconfirm --needed \
  base-devel \
  blas \
  clang \
  gcc \
  git \
  make \
  pcre2 \
  perl \
  python \
  python-pip \
  r \
&& /usr/bin/pip install --upgrade pip \
&& /usr/bin/pip install --no-cache-dir -r requirements.txt \
&& mkdir ~/.R \
&& echo 'CXX=clang++' > ~/.R/Makevars \
&& Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')"

RUN scons

FROM alpine:latest as production

COPY --from=builder /tmp/source/pluma /usr/lib/pluma/pluma
COPY --from=builder /tmp/source/PluGen /usr/lib/pluma/PluGen
COPY --from=builder /tmp/source/plugins /usr/lib/pluma/plugins
COPY --from=builder /tmp/source/pipelines /usr/lib/pluma/pipelines
COPY --from=builder /tmp/source/LICENSE /usr/lib/pluma/LICENSE

ADD https://github.com/just-containers/s6-overlay/releases/latest/download/s6-overlay-amd64.tar.gz ./

RUN tar xf s6-overlay-amd64.tar.gz -C / \
  && rm s6-overlay-amd64.tar.gz \
  && ln -s /usr/lib/pluma/pluma /usr/bin/pluma \
  && ln -s /usr/lib/pluma/PluGen /usr/bin/PluGen \
  && apk add -t .runtime-deps \
    blas \
    clang \
    gcc \
    musl \
    perl \
    R \
    R-mathlib \
    python3 \
  && ln -s /usr/bin/python3 /usr/bin/python \
  && mkdir ~/.R \
  && printf 'CXX=clang++\nPKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')"

VOLUME ["/usr/lib/pluma/plugins"]
VOLUME ["/usr/lib/pluma/pipelines"]

ENTRYPOINT ["/init"]
CMD ["/usr/bin/pluma"]
