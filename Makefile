#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := project

include $(IDF_PATH)/make/project.mk

erase:
	esptool.py --p /dev/tty.SLAB_USBtoUART  erase_flash



