#!/bin/bash

for t in $(ls t/*.t)
do
  echo "Tests : "$t
  prove $t
done

rm -rf t/servroot