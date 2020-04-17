# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
    config.vm.box = "generic/alpine310"

    # config.vm.provider "vmware_desktop" do |v|
    # end

    config.vm.provision "shell", inline: <<-EOF
        echo "https://dl-4.alpinelinux.org/alpine/edge/main\n" \
            "https://dl-4.alpinelinux.org/alpine/v3.11/community" \
            > /etc/apk/repositories
        apk update && apk upgrade && apk add -t .deps \
            blas \
            blas-dev \
            build-base \
            build-base \
            clang \
            gcc \
            git \
            libc-dev \
            libexecinfo-dev \
            libffi \
            libffi-dev \
            musl \
            musl-dev \
            perl \
            perl-dev \
            python3 \
            python3-dev \
            R \
            R-dev \
            R-mathlib
    EOF
end
