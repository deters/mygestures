
sysconf_DATA = mygestures.yaml
dist_sysconf_DATA = mygestures.yaml

udevrulesdir = @udevrulesdir@
udevrules_DATA = 99-mygestures.rules
dist_udevrules_DATA = 99-mygestures.rules

install-data-hook:
	-modprobe uinput 2>/dev/null
	-udevadm control --reload-rules 2>/dev/null
	-udevadm trigger --attr-match=name=uinput 2>/dev/null
	-udevadm trigger --subsystem-match=input 2>/dev/null

#CC=gcc
#CC=clang


SUBDIRS=src
