#!/bin/sh

pidof had & > /dev/null
if [ $? -ne 0 ]
    then
    rm /var/run/had.pid
    echo "had died, restarting" >> /tmp/had.log
    had
fi

