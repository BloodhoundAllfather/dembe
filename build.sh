#!/bin/sh
echo "Build started"
rm -f dembe
gcc -fdiagnostics-color=always -Wdiscarded-qualifiers -lpthread dembe.c -o dembe
echo " "
echo " "
echo "Build is done"
echo "-------------------------------------------------------------------------------"
ls -la
