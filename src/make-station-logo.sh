#!/bin/mksh

# Create station logo data to build into the mpxgen executable for RDS2

if [ -z "$1" ]; then
  echo "Usage: $0 <logo file>"
  exit
fi

if [ -f "$1" ]; then
  cp "$1" /tmp/station_logo
  xxd -i /tmp/station_logo | sed s/_tmp_// > rds2_image_data.c
  rm /tmp/station_logo
fi
