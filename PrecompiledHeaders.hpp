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

#include <OVR_Version.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_CAPI_Util.h>

#include <Ogre.h>
#include <Plugins/OctreeSceneManager/OgreOctreePlugin.h>
#include <Plugins/ParticleFX/OgreParticleFXPlugin.h>
#include <OgreDepthBuffer.h>
#include <GL/glew.h>

// include all includes in OgreGLTexture.h before forcing private to be public
// we don't want to affect other headers
#include <OgreGLPrerequisites.h>
#include <OgrePlatform.h>
#include <OgreRenderTexture.h>
#include <OgreTexture.h>
#include <OgreGLSupport.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreGLStateCacheManager.h>
#include <OgreGLPlugin.h>

// HACK
#define private public
#include <OgreGLTexture.h>
#undef private

// custom

void notice(std::string message);
void error(std::string message);

template <typename T>
T clamp(const T& n, const T& lower, const T& upper) {
  return std::max(lower, std::min(n, upper));
}

