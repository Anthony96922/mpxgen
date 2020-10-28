#!/bin/mksh

# Create station logo data to build into the mpxgen executable for RDS2

if [ -z "$1" ]; then
  echo "Usage: $0 <logo file>"
  exit
fi

if [ -f "$1" ]; then
  ln -s "$1" station_logo
  xxd -i station_logo > rds2_image_data.c
  rm station_logo
fi
