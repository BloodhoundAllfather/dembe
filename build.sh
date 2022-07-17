#!/bin/sh
#gcc -lpthread -Wno-discarded-qualifiers dembe.c -o dembe
gcc -fdiagnostics-color=always -Wdiscarded-qualifiers -lpthread -g dembe.c -o dembe
