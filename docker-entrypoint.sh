#!/usr/bin/env bash

if [[ -x "$(command -v $1 &>/dev/null)" ]]; then
    exec $@
else
    exec /app/pluma $@
fi
