#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <vector>
#include "filesystem.h"
#include "icons.h"


extern int selectedIndex;
extern String currentPath;
extern bool menuNeedsRedraw;


#define MAX_VISIBLE_LINES 8


extern std::vector<FSEntry*> currentEntries;


void initMenu();


void initMenuFromString(const String& fileData);


void renderMenu(bool forceRedraw = false);


void handleMenuSelection();


void menuUp();


void menuDown();


void navigateTo(const String& path);


void goBack();


FSEntry* getSelectedEntry();


int getVisibleItemCount();


void invalidateMenu();

#endif 
