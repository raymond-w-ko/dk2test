#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <string>

#include <SDL.h>
#include <SDL_syswm.h>

#include <OVR.h>
#include <../Src/OVR_CAPI_GL.h>

#include <Ogre.h>

void notice(std::string message);
void error(std::string message);
