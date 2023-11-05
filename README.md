# AI improviser plugin

This is a VST plugin built with the JUCE framework for macOS, Windows and Linux.

If you send it MIDI, it will learn in real-time and improvise with the model.

Wire its MIDI out to a synthesizer to allow it improvise. 

You can download the plugin on the releases page. 

This software is part of a series of improvisation systems I have developed, but it is the first one that comes in the form of a VST plugin.


To cite this work, please using the following: 

```
@inproceedings{matthew_yee-king_conversations_2023,
	address = {Milan},
	title = {Conversations with our {Digital} {Selves}: the development of an autonomous music improviser},
	abstract = {Mark d'Inverno},
	booktitle = {Proc. of the 24th {Int}. {Society} for {Music} {Information} {Retrieval} {Conf}.},
	author = {{Matthew Yee-King}},
	year = {2023},
}
```

## For developers

You can build the plugin using CMake:

```
cmake -B build .
cmake --build build --config Release 
```

That will generate a standalone application in the build folder and it will automatically install the plugin version to the default location.
 
