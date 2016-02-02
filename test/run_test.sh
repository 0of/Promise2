#!/bin/bash

find "$(dirname $0)/bin" -type f -perm +111 -print -exec {} \;
