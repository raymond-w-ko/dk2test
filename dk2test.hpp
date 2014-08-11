#pragma once

class dk2test {
 public:
  struct Eye {
    // 1.0 gets you a render target size that gives you 1.0 center pixels after distortion
    // decrease this if you need FPS
    float RenderQuality;
    // 1.0 is the default FOV
    // last resort, as this will break immersion
    float FovQuality;
    
    ovrFovPort Fov;
    ovrSizei TextureSize;

    Ogre::TexturePtr Texture;
    int TextureId;
    Ogre::RenderTarget* RenderTarget;

    ovrEyeRenderDesc RenderDesc;
  };

  dk2test();
  ~dk2test();

  void* GetNativeWindowHandle();
  void ConfigureRenderingQuality(float render_quality, float fov_quality);

  void CreateScene();
  void AttachSceneToRenderTargets();

  void loop();
  void renderOculusFrame();

 private:
  enum EyeEnum {
    kLeft = 0,
    kRight = 1,
    kNumEyes,
  };

  void initOVR();
  void initSDL();
  void initOgre();

  void createRenderTextureViewer();

  bool mUsingDebugHmd;
  ovrHmd mHmd;

  SDL_Window* mWindow;

  Ogre::Root* mRoot;
  Ogre::RenderWindow* mRenderWindow;

  Eye mEyes[2];

  Ogre::SceneManager* mSceneManager;
  Ogre::SceneNode* mRootNode;
  std::vector<Ogre::Camera*> mEyeCameras;
  Ogre::SceneNode* mHeadSceneNode;
  std::vector<Ogre::AnimationState*> mAnimatingStates;

  Ogre::SceneManager* mDummySceneManager;
};
