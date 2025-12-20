Flipper Zero UI - Storage Directory

This folder is for user data and files that persist on the ESP32's
LittleFS filesystem. Applications and lua scripts are in the apps folder.
Files here won't be run (like with applications) but only be viewed.

Contents:
- User data files

The filesystem is automatically initialized on first boot.
Files placed here during firmware upload will be available
in the /storage/ path on the device.

