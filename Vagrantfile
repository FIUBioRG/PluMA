# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  config.vm.box = "ubuntu/bionic64"

  config.vm.provider "virtualbox" do |vb|
    # Customize the amount of memory on the VM:
    vb.memory = "1024"
  end

  config.vm.provision "shell", inline: <<-SHELL
    apt-get -y update
    apt-get -y install build-essential gcc g++ curl git python3 python3-pip perl \
    libperl-dev swig r-base r-cran-rcpp r-cran-rinside python3-pkgconfig
    pip3 install scons
  SHELL
end
