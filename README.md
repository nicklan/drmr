DrMr
====

DrMr is an LV2 sampler plugin.  It's main reason to exist is to give a way for lv2 hosts to have a built in drum synth that can save its entire state (i.e. no need to go out to external tools and no need to save extra state).  See the wiki (click the wiki tab above) for some screenshots.  DrMr currently supports the following:

- Control via midi
- Scan for and load hydrogen drum kits (see note 2)
- Multi-layer hydrogen kits (will pick layer based on that samples set gain)
- LV2 controls for gain on first 32 samples of kit (see note 1 below)
- LV2 controls for pan on first 32 samples of kit (see note 1 below)
- GTK ui that can select a kit and control gain/pan on each sample
- Custom knob widget for GTK ui based on phatknob that is both functional and awesome looking. (see wiki for screenshot)
- Sample grid can start in any corner of the window, to match the layout of your controller.

Hopefully coming soon:

- Creating / Saving custom kits on a per sample basis using the GTK UI
- ASDR envelope on samples (Will be automatable and controllable in the UI)


DrMr is a new project, so the code should be considered alpha.  Bug reports are much appreciated.

Regarding This Branch
---------------------

This (lv2unstable) branch of DrMr uses the lv2-atom and lv2-state extensions, which are both experimental lv2 extensions.  As such, you'll need to have the svn version of lv2 installed (see [here](http://lv2plug.in/trac/wiki/SVN_repository) for info about how to install lv2 from svn).  You'll also need a host that supports these extensions.  Known working hosts are [jalv](http://drobilla.net/software/jalv/) and [ardour3](http://ardour.org/download_full), both from svn (i.e. Ardour3-beta3 won't work).  Using these new extensions allows DrMr to have a number of new and planned features.

The intention is to have this version become the main stable version as the lv2 spec stabilizes and more hosts support the required extensions.

New features include:

- Kits are now selected via their path and not the hacky integer port as before
- The kit path is now saved and restored using lv2-state, so you can install/remove/modify hydrogen kits and your state won't be messed up.
- No need to scan kits in the plugin core so the plugin loads faster
- Led indicator in the UI when a sample is triggered
- Led indicator can be clicked to trigger sample for preview

Coming soon features:

- Loading of individual samples into each slot (so you can tweak the kit to use your own samples).  New sample locations will also be saved using lv2-state so modifications will be persistent.
- New kit creation and saving (a bit further down the road)


One thing to note is that the kit index port has been removed.  This means if you have projects using the master branch version of plugin you should note the kit externally as that state will not be reloaded.

Download
--------
Only via git for now, just check out this repo

Compilation and Install
-----------------------
DrMr is built with [CMake](http://www.cmake.org).

To build it simply do (from this dir):

    mkdir build
    cd build
    cmake ..    (or "cmake -DUSE_NKNOB=OFF .." if you want old style sliders)

Then do:

    make
    make install

to install.  There are some customizable variables for cmake.  To see them do "cmake -L".  The important ones are:

USE_NKNOB - Use custom knob widget for controls instead of the default gtk sliders.  This defaults to ON.  Try turning it off if you are experiencing problems, or just prefer the sliders.

SAMP_ZERO_POS - Controls where sample zero will position itself in the sample table.  This is just the default value, and can be changed in the ui when DrMr is running.  Valid values are:

    0 - Top Left (default)
    1 - Bottom Right (This will align with many drum machines and MIDI pads)
    2 - Top Right
    3 - Bottom Right
Any other value will emit a warning and use 0.

LV2_INSTALL_DIR - The directory to install the DrMr plugin to. To install to your home directory, use "~/.lv2" and clear the CMAKE_INSTALL_PREFIX. This defaults to "lib/lv2" (this is relative to CMAKE_INSTALL_PREFIX, which is usually /usr/local)

You can also use "ccmake .." or "cmake-gui .." for a more interactive configuration process.

A legacy Makefile is included, that will possibly work for you if you don't want to use cmake.  To use it just do (from this dir):

    make -f Makefile.legacy
    make -f Makefile.legacy install

You'll need the following libraries to build and install DrMr:

- [libsndfile](http://www.mega-nerd.com/libsndfile/)
- [libsamplerate](http://www.mega-nerd.com/SRC/index.html)
- [lv2](http://lv2plug.in/)
- [libexpat](http://expat.sourceforge.net)
- [gtk+](http://www.gtk.org)

DrMr scans the following directories for hydrogen drum kits:

- /usr/share/hydrogen/data/drumkits/
- /usr/local/share/hydrogen/data/drumkits/
- /usr/share/drmr/drumkits/
- ~/.hydrogen/data/drumkits/
- ~/.drmr/drumkits/

If you want to add others, add them to the default_drumkit_locations array at the top of drmr_hydrogen.c

### Note 1
DrMr is currently using a static ttl file.  This means I have to decide statically how many gain/pan controls to expose.  I've settled on 32 for the moment, but that is arbitrary.  At some point DrMr will probably move to using the LV2 Dynamic Manifest feature to expose the appropriate number of gain controls for the current sample set, although how force a host update of the manifest when the kit is changed is unclear (if you know how, please let me know)

### Note 2
DrMr only currently supports a subset of things that can be specified in a hydrogen drumkit.xml file.  Specifically, DrMr will not use gain/pan/pitch/asdr information.  DrMr basically only uses the filename and layer min/max information to build it's internal sample representation.  Values specified in .xml files will be used as DrMr begins to support the features needed for those values to make sense.