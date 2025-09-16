#ifndef __KEYCODES_H__
#define __KEYCODES_H__

#ifdef _WIN32
#include "keycodes_winapi.h"
#elif __linux__
#include "keycodes_linux.h"
#else
#endif

#endif