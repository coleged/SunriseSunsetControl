#!/bin/sh
#
# lushscpt.sh	sheduled at job by lush.sh which is run from cron
#
echo "" | mail -s "outside lights on" colege@gmail.com
(/usr/local/bin/lutctrl /usr/local/etc/outside_on.lut 2>&1)&
