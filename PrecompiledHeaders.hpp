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
#include <boost/lexical_cast.hpp>

// third party

#include <SDL.h>
#include <SDL_syswm.h>

#include <OVR.h>
#include <../Src/OVR_CAPI_GL.h>
#include <../Src/CAPI/CAPI_HSWDisplay.h>

#include <Ogre.h>

// custom

void notice(std::string message);
void error(std::string message);

template <typename T>
T clamp(const T& n, const T& lower, const T& upper) {
  return std::max(lower, std::min(n, upper));
}

