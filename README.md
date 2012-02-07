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

### Note 1
As stated above, a goal of DrMr is to have the host save all the state for you.  As such, the current kit needs to be a control.  Unfortunatly, string controls in LV2 are experiemental at the moment, and not supported by many hosts (in particular ardour doesn't support them).  This means the kit needs to be set via a numeric control.  DrMr specifies an integer index as a control to specify which kit to load.  A kits index is the order in which is was found.  This means changing, adding, or removing hydrogen kits could mess up your saved index.  Sorry.

To figure out which kit is being loaded, have a look at the ouput of your host, DrMr will print out info about the kit it is loading.  In the future, current kit information will be available in the GTK ui.

### Note 2
DrMr is currently using a static ttl file.  This means I have to decide apriori how many gain controls to expose.  I've settled on 16 for the moment, but that is arbitrary.  At some point DrMr will probably move to using the LV2 Dynamic Manifest feature to expose the appropriate number of gain controls for the current sample set