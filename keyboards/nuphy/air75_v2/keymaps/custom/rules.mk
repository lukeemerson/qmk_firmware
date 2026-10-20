WPM_ENABLE = yes
RAW_ENABLE = yes

# leader legend codes come from the private snippets repo when it's on this machine
LEADER_SRC := $(HOME)/.config/snippets/build.py
ifneq ($(wildcard $(LEADER_SRC)),)
    $(shell python3 $(LEADER_SRC) --header $(dir $(lastword $(MAKEFILE_LIST)))leader_codes.h)
endif
