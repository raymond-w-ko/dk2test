#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

dk2test::dk2test() {
  this->initOVR();
  this->initSDL();

  this->initOVR2();
}

dk2test::~dk2test() {
  this->destroySDL();
  this->destroyOVR();
}

void dk2test::initOVR() {
  ovr_Initialize();

  mHmd = ovrHmd_Create(0);
  if (!mHmd) {
    notice("Could not find a real Oculus head mounted display! Creating a debug one based on the DK2.");
    mHmd = ovrHmd_CreateDebug(ovrHmd_DK2);
  }

  mRecommendedTexSize[kLeft] = ovrHmd_GetFovTextureSize(
      mHmd, ovrEye_Left, mHmd->DefaultEyeFov[0], 1.0f);
  mRecommendedTexSize[kRight] = ovrHmd_GetFovTextureSize(
      mHmd, ovrEye_Right, mHmd->DefaultEyeFov[1], 1.0f);

  mRenderTargetSize.w = mRecommendedTexSize[kLeft].w + mRecommendedTexSize[kRight].w;
  mRenderTargetSize.h = std::max(mRecommendedTexSize[kLeft].h, mRecommendedTexSize[kRight].h);

  // TODO: does this affect quality? what is the performance impact?
  mEyeRenderMultisample = 1;

  static const unsigned int supportedTrackingCaps = 
      ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;
  static const unsigned int requiredTrackingCaps = 0;
  ovrHmd_ConfigureTracking(mHmd, supportedTrackingCaps, requiredTrackingCaps);
}

void dk2test::destroyOVR() {
  ovrHmd_Destroy(mHmd);
  mHmd = NULL;

  ovr_Shutdown();
}

void dk2test::initSDL() {
  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  static const unsigned int context_flags = 
      0 |
      //SDL_GL_CONTEXT_DEBUG_FLAG |
      // no deprecated functionality, probably faster
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG |
      // safer APIs
      //SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG |
      // require the GL to make promises about what to do in the face of driver
      // or hardware failure.
      //SDL_GL_CONTEXT_RESET_ISOLATION_FLAG
      0
      ;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

  static const unsigned int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
  mWindow = SDL_CreateWindow(
      "dk2test",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      //this->mHmd->Resolution.w, this->mHmd->Resolution.h,
      1920, 1080,
      flags
      );
  if (!mWindow) {
    error("Could not create SDL2 window!");
  }

  mGLContext = SDL_GL_CreateContext(mWindow);
  if (!mGLContext) {
    error("Could not create OpenGL context!");
  }
}

void dk2test::initOVR2() {
  SDL_SysWMinfo window_info;
  SDL_GetWindowWMInfo(mWindow, &window_info);

  ovrGLConfig cfg;
  cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
  cfg.OGL.Header.RTSize = OVR::Sizei(mHmd->Resolution.w, mHmd->Resolution.h);
  cfg.OGL.Header.Multisample = mEyeRenderMultisample;
#ifdef _WIN32
  cfg.OGL.Window = window_info.info.win.window;
  cfg.OGL.DC = NULL;
#endif

  int distortionCaps = 0;
  ovrEyeRenderDesc eyeRenderDesc[2];
  
  ovrBool result = ovrHmd_ConfigureRendering(
      mHmd, &cfg.Config, distortionCaps,
      mHmd->DefaultEyeFov, eyeRenderDesc);
}

void dk2test::destroySDL() {
  SDL_DestroyWindow(mWindow);
  SDL_Quit();
}

void dk2test::loop() {
  SDL_Event e;
  bool quit = false;

  while (!quit) {
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          quit = true;
      }
    }

    // render here
  }
}
