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

  void changeToDirectoryOfExecutable();

  void initOVR();
  void initSDL();
  void initOVR2();
  void initOgre();

  void destroyOgre();
  void destroySDL();
  void destroyOVR();

  void createScene();
  void createRenderTextureViewer();

  void* getNativeWindowHandle();

  ovrHmd mHmd;
  SDL_Window* mWindow;

  Ogre::Root* mRoot;
  Ogre::RenderWindow* mRenderWindow;

  OVR::Sizei mRecommendedTexSize[2];
  OVR::Sizei mRenderTargetSize;
  int mEyeRenderMultisample;
  Ogre::TexturePtr mEyeRenderTexture;
  Ogre::RenderTarget* mEyeRenderTarget;

  Ogre::SceneManager* mSceneManager;
  Ogre::SceneNode* mRootNode;
  std::vector<Ogre::Entity*> mAnimatingEntities;

  Ogre::SceneManager* mDummySceneManager;
};
