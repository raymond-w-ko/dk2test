// standard C
#include <stdlib.h>
#include <stdio.h>
#include <direct.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#endif

// standard C++

#include <string>
#include <vector>

#include <boost/filesystem.hpp>

// third party

#include <SDL.h>
#include <SDL_syswm.h>

#include <OVR.h>
#include <../Src/OVR_CAPI_GL.h>

#include <Ogre.h>

// custom

void notice(std::string message);
void error(std::string message);
