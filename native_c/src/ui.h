#ifndef ZAKON_UI_H
#define ZAKON_UI_H
#include <windows.h>
#include "settings.h"
#include "sqlite_api.h"
int zd_gui_run(HINSTANCE instance, ZDPaths *paths, ZDSettings *settings, ZDSqliteApi *sqlite);
#endif
