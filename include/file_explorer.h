#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <Arduino.h>
#include "app_runner.h"
#include <string>




void setFileExplorerRoot(const char* path);


std::string getFileExplorerPath();


AppState fileExplorerApp();

#endif 
