#
# Regular cron jobs for the mygestures package
#
0 4	* * *	root	[ -x /usr/bin/mygestures_maintenance ] && /usr/bin/mygestures_maintenance
