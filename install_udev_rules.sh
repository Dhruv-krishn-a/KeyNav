#!/bin/bash
# install_udev_rules.sh
# Sets up permissions so KeyNav can use evdev and uinput without sudo.

set -e

if [ "$EUID" -ne 0 ]; then
  echo "Please run this script as root (sudo ./install_udev_rules.sh)"
  exit 1
fi

echo "Creating udev rules for input and uinput devices..."

cat << 'EOF' > /etc/udev/rules.d/99-keynav-input.rules
# Allow members of the "input" group to read keyboard events
KERNEL=="event*", SUBSYSTEM=="input", GROUP="input", MODE="0660"

# Allow members of the "input" group to create virtual devices via uinput
KERNEL=="uinput", SUBSYSTEM=="misc", GROUP="input", MODE="0660"
EOF

# Ensure the 'input' group exists
groupadd -f input

# Add the current SUDO_USER to the input group
if [ -n "$SUDO_USER" ]; then
    usermod -aG input "$SUDO_USER"
    echo "Added user '$SUDO_USER' to the 'input' group."
    echo "IMPORTANT: You must log out and log back in for the group change to take effect."
else
    echo "WARNING: Could not detect your user. Please run: sudo usermod -aG input YOUR_USERNAME"
fi

echo "Reloading udev rules..."
udevadm control --reload-rules
udevadm trigger

echo "Done! After logging out and back in, KeyNav will run without sudo."
