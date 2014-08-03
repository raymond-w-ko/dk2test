#pragma once

class dk2test {
 public:
  dk2test();
  ~dk2test();
  void loop();

 private:
  enum EyeEnum {
    kLeft = 0,
    kRight = 1,
    kNumEyes,
  };

  void initOVR();
  void initSDL();
  void initOVR2();

  void destroySDL();
  void destroyOVR();

  ovrHmd mHmd;
  SDL_Window* mWindow;
  SDL_GLContext  mGLContext;

  OVR::Sizei mRecommendedTexSize[2];
  OVR::Sizei mRenderTargetSize;
  int mEyeRenderMultisample;
};
