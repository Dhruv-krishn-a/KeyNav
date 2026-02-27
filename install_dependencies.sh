#!/bin/bash
# install_dependencies.sh
# Detects the Linux distribution and installs required packages for KeyNav

set -e

if [ "$EUID" -ne 0 ]; then
  echo "Please run this script as root (sudo ./install_dependencies.sh)"
  exit 1
fi

if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    OS_LIKE=$ID_LIKE
else
    echo "Cannot determine OS. Please install dependencies manually."
    exit 1
fi

echo "Detected OS: $OS"

if [[ "$OS" == "ubuntu" || "$OS" == "debian" || "$OS_LIKE" == *"ubuntu"* || "$OS_LIKE" == *"debian"* ]]; then
    echo "Installing for Debian/Ubuntu based system..."
    apt-get update
    apt-get install -y build-essential cmake pkg-config libx11-dev libxtst-dev libxrandr-dev libcairo2-dev libgtk-3-dev libgtk-layer-shell-dev

elif [[ "$OS" == "fedora" || "$OS" == "rhel" || "$OS_LIKE" == *"fedora"* ]]; then
    echo "Installing for Fedora/RHEL based system..."
    dnf install -y gcc-c++ cmake pkgconf-pkg-config libX11-devel libXtst-devel libXrandr-devel cairo-devel gtk3-devel gtk-layer-shell-devel

elif [[ "$OS" == "arch" || "$OS" == "manjaro" || "$OS_LIKE" == *"arch"* ]]; then
    echo "Installing for Arch based system..."
    pacman -S --needed --noconfirm base-devel cmake pkgconf libx11 libxtst libxrandr cairo gtk3 gtk-layer-shell

else
    echo "Unsupported OS: $OS. Please refer to CMakeLists.txt and install dependencies manually."
    exit 1
fi

echo "Dependencies installed successfully!"
