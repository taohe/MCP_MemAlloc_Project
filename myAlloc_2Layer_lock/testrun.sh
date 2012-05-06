#!/bin/bash

i=30

while [ "$i" != "0" ]; do
  make runtest3 | grep "Heap" 
#  printf "i = %d\n" $i
  i=$((i - 1))
  sleep 1s
done
