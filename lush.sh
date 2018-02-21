#!/bin/sh
# 
#	lush.sh   -  lumin sheduler (Lutron Lighting control)
#
#	Use in conjunction with sunrise to shedule events based on
#	sunrise/set times
#
#	Ed Cole Jan 2018
#
# run this at midday to set up at job for later

# Schedule job 0 hours before sunset
time=`/usr/local/bin/sunrise -e -h -0 -t "%H:%M"`
script="/usr/local/bin/lushscpt.sh"
cmd="at -f $script $time"

# run the at command to schedule the job
$cmd 
