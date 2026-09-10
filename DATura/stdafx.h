// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define NOMINMAX          // prevent windows.h from defining min/max macros

#include <windows.h>
#include <stdio.h>
// C RunTime Header Files
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Define the alias after CRT declarations, otherwise the deprecated stricmp
// declaration is rewritten into (and marks deprecated) _stricmp itself.
#define stricmp _stricmp

#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")

// TODO: reference additional headers your program requires here
