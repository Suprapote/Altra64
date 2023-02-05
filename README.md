Alternative Everdrive64 menu

`Altra64` is an open source menu for [Everdrive64](http://krikzz.com/) and ed64+ and is based on a fork of alt64 which was
originally written by saturnu, and released on the
[Everdrive64 forum](http://krikzz.com/forum/index.php?topic=816.0).
## Setup

1, Format your SD card to fat32.

2, Extract ED64P.zip into the rout of your sd card.

3, Change ALT64.ini To your hearts content.

4, Add leagally obtained ROMs

5, Insert the SD card into the ED64plus and enjoy

## Controls

```
Key settings
L brings up the mempak menu
      B abort
      A backup
      R format
      Z view controller pak
Z about screen
A start rom/directory/mempak
B back/cancel
START start last rom
C-left rom info / mempak content view
C-right rom config  screen
C-up view full filename
C-down Toplist 15
L+R delete files
```

## Building

Install docker using the following command:
```
sudo apt-get install docker.io
```
and execute:

```
sudo dockerd
```

### Build `Altra64`

To build the R.O.M.

run the build.sh script

or this command:
```
sudo docker run --rm -v "$(pwd):/build" ghcr.io/ariahiro64/altra64:master make
```

If it all worked, you will find `OS64P.v64` in the `bin` directory.

### Clean `Altra64`

Finally, we can clean the build objects from the project

from the projects root directory

```
sudo docker run --rm -v "$(pwd):/build" ghcr.io/ariahiro64/altra64:master make clean
```

### Big thanks to the countless people who make altra64 possible!!! Enjoy!
