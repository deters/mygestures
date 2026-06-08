
sysconf_DATA = mygestures.yaml
dist_sysconf_DATA = mygestures.yaml

udevrules_DATA = 99-mygestures.rules
dist_udevrules_DATA = 99-mygestures.rules

install-data-hook:
	-udevadm control --reload-rules 2>/dev/null
	-udevadm trigger 2>/dev/null

#CC=gcc
#CC=clang


SUBDIRS=src
