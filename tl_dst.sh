#!/usr/bin/env bash

echo "" > koh_src/tl_dst.h 
fd ".*\.lua" tl_dst --exec xxd -i {} >> koh_src/tl_dst.h 

