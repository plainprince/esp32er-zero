#ifndef LUA_APP_H
#define LUA_APP_H

#include <Arduino.h>
#include "app_runner.h"


void initLua();


void setLuaScript(const char* script);


void setLuaScriptFromFile(const char* path);


AppState luaApp();




























#endif 

