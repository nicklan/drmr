DrMr
====

DrMr is an LV2 sampler plugin.  It's main reason to exist is to give a way for lv2 hosts to have a built in drum synth that can save its entire state (i.e. no need to go out to external tools and no need to save extra state).  See the wiki (click the wiki tab above) for some screenshots.  DrMr currently supports the following:

- Control via midi
- Scan for and load hydrogen drum kits (see note 3)
- Multi-layer hydrogen kits (will pick layer based on that samples set gain)
- Kit is set via an LV2 control (see note 1 below)
- LV2 controls for gain on first 32 samples of kit (see note 2 below)
- LV2 controls for pan on first 32 samples of kit (see note 2 below)
- GTK ui that can select a kit and control gain/pan on each sample

Hopefully coming soon:

- Creating / Saving custom kits on a per sample basis using the GTK UI
- ASDR envelope on samples (Will be automatable and controllable in the UI)


DrMr is a new project, so the code should be considered alpha.  Bug reports are much appreciated.

Download
--------
Only via git for now, just check out this repo

Compilation and Install
-----------------------
You'll need the following libraries to build and install DrMr:

- [libsndfile](http://www.mega-nerd.com/libsndfile/)
- [libsamplerate](http://www.mega-nerd.com/SRC/index.html)
- [lv2](http://lv2plug.in/)
- [libexpat](http://expat.sourceforge.net)

The Makefile has the INSTALL_DIR and CC flags hard coded to /usr/local/lib/lv2 and gcc at the moment.  Edit those if you want to change that.

DrMr scans the following directories for hydrogen drum kits:

- /usr/share/hydrogen/data/drumkits/
- /usr/local/share/hydrogen/data/drumkits/
- /usr/share/drmr/drumkits/
- ~/.hydrogen/data/drumkits/
- ~/.drmr/drumkits/

If you want to add others, add them to the default_drumkit_locations array at the top of drmr_hydrogen.c

### Note 1
As stated above, a goal of DrMr is to have the host save all the state for you.  As such, the current kit needs to be a control.  Unfortunately, string controls in LV2 are experimental at the moment, and not supported by many hosts (in particular ardour doesn't support them).  This means the kit needs to be set via a numeric control.  DrMr specifies an integer index as a control to select which kit to load.  A kits index is the order in which is was found.  This means changing, adding, or removing hydrogen kits could mess up your saved index.  Sorry.

You can figure out which kit is loaded by looking in the GtkUI at the bottom, or look at the print output from your host, as drmr will print the names of kits as it loads them.

### Note 2
DrMr is currently using a static ttl file.  This means I have to decide statically how many gain/pan controls to expose.  I've settled on 32 for the moment, but that is arbitrary.  At some point DrMr will probably move to using the LV2 Dynamic Manifest feature to expose the appropriate number of gain controls for the current sample set, although how force a host update of the manifest when the kit is changed is unclear (if you know how, please let me know)

### Note 3
DrMr only currently supports a subset of things that can be specified in a hydrogen drumkit.xml file.  Specifically, DrMr will not use gain/pan/pitch/asdr information.  DrMr basically only uses the filename and layer min/max information to build it's internal sample representation.  Values specified in .xml files will be used as DrMr begins to support the features needed for those values to make sense.