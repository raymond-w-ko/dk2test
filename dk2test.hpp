#pragma once

typedef Ogre::GL3PlusRenderSystem TheRenderSystem;
typedef Ogre::GL3PlusPlugin TheRenderSystemPlugin;
typedef Ogre::GL3PlusTexture TheRenderSystemTexture;
  
class dk2test {
 public:
  struct Eye {
    // 1.0 gets you a render target size that gives you 1.0 center pixels after
    // distortion. Decrease this if you need FPS
    float RenderQuality;
    // 1.0 is the default FOV
    // last resort, as this will break immersion by reduce the field of view
    float FovQuality;
    
    ovrFovPort Fov;
    ovrSizei TextureSize;
    ovrEyeRenderDesc RenderDesc;
    
    ovrSwapTextureSet* SwapTextureSet;
    struct Textures_ {
      Ogre::TexturePtr Texture;
      Ogre::RenderTarget* RenderTarget;
    } Textures[2];
    
    Ogre::CompositorWorkspace* CompositorWorkspaces[2];
  };
  
  ovrTexture* mOvrMirrorTexture;
  Ogre::TexturePtr mMirrorTexture;
  
  static void onOculusSDKLogMessage(uintptr_t userData,
                                    int level, const char* message);

  dk2test();
  ~dk2test();

  void onOculusSDKLogMessage(int level, const char* message);

  void* GetNativeWindowHandle();
  void CreateEyeRenderTargets(float render_quality, float fov_quality);

  void CreateScene();
  void SetupCompositor();
  void SetupMirroring();

  void loop();
  void renderOculusFrame();

 private:
  void initOVR();
  void initSDL();
  void initOgre();
  void loadAssets();

  void _deleteShimmedOgreTexture(Ogre::TexturePtr& texture_ptr);

  ovrHmd mHmd;
  ovrHmdDesc mHmdDesc;
  // not used as of 0.7.0 SDK
  ovrGraphicsLuid mGraphicsLuid;

  SDL_Window* mWindow;

  Ogre::Root* mRoot;
  TheRenderSystem* mRenderSystem;
  Ogre::RenderWindow* mRenderWindow;

  Eye mEyes[ovrEye_Count];

  Ogre::SceneManager* mSceneManager;
  Ogre::SceneNode* mRootNode;
  std::vector<Ogre::Camera*> mEyeCameras;
  Ogre::SceneNode* mHeadSceneNode;
  std::vector<Ogre::AnimationState*> mAnimatingStates;
  int mWorkspaceRenderOrder;

  Ogre::SceneManager* mDummySceneManager;
  Ogre::CompositorWorkspace* mMirrorWorkspace;
};
