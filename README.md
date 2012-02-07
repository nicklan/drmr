DrMr
====

DrMr is an LV2 sampler plugin.  It's main reason to exist is to give a way for lv2 hosts to have a built in drum synth that can save its entire state (i.e. no need to go out to external tools and no need to save extra state).  It currently supports the following:

- Control via midi
- Scan for and load hydrogen drum kits
- Kit is set via an LV2 control (see note 1 below)
- LV2 controls for gain on first 16 samples of kit (see note 2 below)

Hopefully coming soon:

- GTK ui for kit info / loading other kits / sample tweaking
- Creating / Saving custom kits on a per sample basis using the GTK UI
- ASDR envelope on samples

Download
--------
Only via git for now, just check out this repo

Compilation and Install
-----------------------
You'll need the following libraries to build and install DrMr:

- [libsndfile](http://www.mega-nerd.com/libsndfile/)
- [lv2](http://lv2plug.in/)
- [libexpat](http://expat.sourceforge.net)

The Makefile has the INSTALL_DIR and CC flags hard coded to /usr/local/lib/lv2 and gcc at the moment.  Edit those if you want to change that.

DrMr scans the following directories for hydrogen drum kits:

- /usr/share/hydrogen/data/drumkits/
- /usr/local/share/hydrogen/data/drumkits/
- ~/.hydrogen/data/drumkits/

If you want to add others, add them to the default_drumkit_locations array at the top of drmr_hydrogen.c
