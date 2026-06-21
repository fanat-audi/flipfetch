#!/bin/bash

set -e

echo "cloning flipfetch repository..."
git clone https://github.com/fanat-audiflipfetch.git /tmp/flipfetch

cd /tmp/flipfetch

echo "compiling flipfetch..."
make

echo "installing flipfetch (requires sudo)..."
sudo make install

echo "done. run 'flipfetch' to test."