#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <vector>
#include <string>
#include "filesystem.h"
#include "icons.h"


extern int selectedIndex;
extern std::string currentPath;
extern bool menuNeedsRedraw;


#define MAX_VISIBLE_LINES 8


extern std::vector<FSEntry> currentEntries;


void initMenu();


void initMenuFromString(const std::string& fileData);


void renderMenu(bool forceRedraw = false);


void handleMenuSelection();


void menuUp();


void menuDown();


void navigateTo(const std::string& path);


void goBack();


FSEntry* getSelectedEntry();


int getVisibleItemCount();


void invalidateMenu();

#endif 
