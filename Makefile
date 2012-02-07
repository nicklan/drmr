BUNDLE = lv2pftci-drmr.lv2
INSTALL_DIR = /home/nick/.lv2
CC=gcc

$(BUNDLE): manifest.ttl drmr.ttl drmr.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl drmr.ttl drmr.so $(BUNDLE)

drmr.so: drmr.c drmr_hydrogen.c
	$(CC) -g -shared -fPIC -DPIC drmr.c drmr_hydrogen.c `pkg-config --cflags --libs lv2-plugin sndfile` -lexpat -lm -o drmr.so

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) drmr.so 