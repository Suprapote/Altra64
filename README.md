# Altra64
[![Build and release](https://github.com/ariahiro64/altra64/actions/workflows/build-release.yml/badge.svg)](https://github.com/ariahiro64/altra64/actions/workflows/build-release.yml)
[![Build docker dev build image](https://github.com/ariahiro64/altra64/actions/workflows/docker-image.yml/badge.svg)](https://github.com/ariahiro64/altra64/actions/workflows/docker-image.yml)

Alternative Everdrive64 menu

`Altra64` is an open source menu for [Everdrive64](http://krikzz.com/) and ED64+<br>
Based on [a fork of alt64](https://github.com/parasyte/alt64) which was originally written by saturnu, and released on the [Everdrive64 forum](http://krikzz.com/forum/index.php?topic=816.0).

## Setup
1. Format your SD card to FAT32.
2. Extract the contents of ALTRA64.zip into the root of your SD card.
3. Change ALT64.ini to your hearts content.
4. Add legally obtained ROMs.
5. Insert the SD card into the ED64plus and enjoy!

## Controls
```
[L] - Opens MEMPAK menu
      [B] - Abort
      [A] - Backup
      [R] - Format
      [Z] - View Controller Pak
[Z] - About Screen
[A] - Start ROM / Open Directory / View MEMPAK
[B] - Back / Cancel
[START] - Start Last ROM
[C-Left] - ROM Info / MEMPAK Content View
[C-Right] - ROM Config Screen
[C-Up] - View Full Filename
[C-Down] - TOP 15 List
[L+R] - Delete File
```

## Building
[Automated Builds Here](https://github.com/bakapear/altra64/actions)<br>
[Docker Image](https://github.com/ariahiro64/altra64/pkgs/container/altra64)


Run in project root to build project:
```sh
sudo docker run --rm -v "$(pwd):/build" ghcr.io/ariahiro64/altra64:master make
```
Output: bin/OS64P.v64

Run in project root to clean project from build objects:
```sh
sudo docker run --rm -v "$(pwd):/build" ghcr.io/ariahiro64/altra64:master make clean
```

### Big thanks to the countless people who make Altra64 possible!!! Enjoy!
