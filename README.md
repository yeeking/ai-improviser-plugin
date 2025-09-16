# AI improviser plugin

This is a VST plugin built with the JUCE framework for macOS, Windows and Linux.

If you send it MIDI, it will learn in real-time and improvise with the model.

Wire its MIDI out to a synthesizer to allow it improvise. 

You can download the plugin on the [releases page][https://github.com/yeeking/ai-improviser-plugin/releases]

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

## For users

You can use the plugin in your VST host. Download the release from the releases area on the right of the page

### Windows

* Copy the .vst file to C:\Program Files\Common Files\VST3
* Your host should scan and find it

### macOS

* Quit the VST host if it is running [logic/ etc.]
* Copy the .vst file to this folder: /Library/Audio/Plug-Ins/VST3
* Open a terminal and run this command, which will allow unsigned software to run: 

```
sudo spctl --master-disable
```

* Launch your VST host and let it scan 
* Verify you add the ai-improviser plugin to your host
* Run this command in the terminal to re-enable blocking of unsigned software:

```
sudo spctl --master-enable
```


### Linux

Probably best to build it yourself! 



## For developers

You can build the plugin using CMake:

```
git clone https://github.com/juce-framework/JUCE.git
cmake -B build .
cmake --build build --config Release -j 8 # -j sets thread count for unixers
```

That will generate a standalone application in the build folder and it will automatically install the plugin version to the default location.

 
