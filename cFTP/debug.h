#pragma once
#define _CRT_SECURE_NO_WARNINGS
#if (_DEBUG && (_WIN32 || _WIN64))
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
