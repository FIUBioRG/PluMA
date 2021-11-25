#!/usr/bin/env bash

if [ -x "$(command -v $1)" ]; then
    exec $@
else
    exec /app/pluma $@
fi
