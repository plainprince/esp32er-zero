#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <Arduino.h>
#include "app_runner.h"




void setFileExplorerRoot(const char* path);


String getFileExplorerPath();


AppState fileExplorerApp();

#endif 

