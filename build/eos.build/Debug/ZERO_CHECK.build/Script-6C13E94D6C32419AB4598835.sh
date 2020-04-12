#!/bin/sh
make -C /Users/keelia/Documents/GitHub/eos/build -f /Users/keelia/Documents/GitHub/eos/build/CMakeScripts/ZERO_CHECK_cmakeRulesBuildPhase.make$CONFIGURATION OBJDIR=$(basename "$OBJECT_FILE_DIR_normal") all
