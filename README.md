# ThirtyDollarSongs

This branch contains the source code of midi2thirtydollar.

This software translates midi files into 🗿 files that can be played at https://thirtydollar.website/.

## Input
It does **NOT** support every midi file and it does also **NOT** support every midi file feature.

It has been manually crafted to _behave well_ with the midi files that can be found in [some of pret's repositories](https://github.com/pret/pokeemerald/tree/master/sound/songs/midi).

## Compilation
It can be compiled with Qt Creator, altough it's plain c++ and you should be able to build it with some manual commands just as well.

## Usage
```midi2thirtydollar <input file> [output file]```

Not specifying the output file will send the output to stdout. Some warnings may appear, but they are sent on stderr.

## Customization
You can customize the instrument maps by changing the map that can be found [here](https://github.com/EssGeeEich/ThirtyDollarSongs/blob/main/main.cpp#L362).

This map makes use of the "availableInstruments" map that can be found slightly above it [(here)](https://github.com/EssGeeEich/ThirtyDollarSongs/blob/main/main.cpp#L270).
