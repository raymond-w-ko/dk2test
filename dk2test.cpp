#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

using namespace Ogre;

dk2test::dk2test()
    : mHmd(nullptr),
      mOvrMirrorTexture(nullptr),
      mWorkspaceRenderOrder(0)
{
  memset(&mGraphicsLuid, 0, sizeof(mGraphicsLuid));
  memset(&mHmdDesc, 0, sizeof(mHmdDesc));

  mEyes[ovrEye_Left].SwapTextureSet = nullptr;
  mEyes[ovrEye_Right].SwapTextureSet = nullptr;

  this->initOVR();
  this->initSDL();
  this->initOgre();
  this->loadAssets();
}

dk2test::~dk2test() {
  if (mOvrMirrorTexture)
    ovr_DestroyMirrorTexture(mHmd, mOvrMirrorTexture);
  _deleteShimmedOgreTexture(mMirrorTexture);

  for (int i = 0; i < 2; ++i) {
    ovr_DestroySwapTextureSet(mHmd, mEyes[i].SwapTextureSet);
  }
  for (int i = 0; i < 2; ++i) {
    _deleteShimmedOgreTexture(mEyes[0].Textures[i].Texture);
    mEyes[0].Textures[i].RenderTarget = nullptr;

    _deleteShimmedOgreTexture(mEyes[1].Textures[i].Texture);
    mEyes[1].Textures[i].RenderTarget = nullptr;
  }
  delete mRoot;
  mRoot = nullptr;

  SDL_DestroyWindow(mWindow);
  SDL_Quit();

  ovr_Destroy(mHmd);
  mHmd = nullptr;
  ovr_Shutdown();
}

void dk2test::_deleteShimmedOgreTexture(Ogre::TexturePtr& texture_ptr) {
  if (!texture_ptr.get())
    return;
  auto texture = static_cast<TheRenderSystemTexture*>(texture_ptr.get());
  // HACK BEGIN
  texture->mSurfaceList.clear();
  texture->mTextureID = 0;
  // HACK END
  std::string texture_name(texture_ptr->getName());
  texture_ptr.setNull();
  Ogre::TextureManager::getSingleton().remove(texture_name);
}

void dk2test::initOVR() {
  ovrInitParams init_params;

  init_params.Flags = 0;
  /// When a debug library is requested, a slower debugging version of the
  //library will run which can be used to help solve problems in the
  //library and debug application code.
  /* init_params.Flags |= ovrInit_Debug; */
  /* init_params.Flags |= ovrInit_ServerOptional; */
  /* init_params.Flags |= ovrInit_RequestVersion; */
  init_params.RequestedMinorVersion = 0;
  init_params.LogCallback = dk2test::onOculusSDKLogMessage;
  init_params.UserData = (decltype(init_params.UserData))this;
  // 0 is default timeout
  init_params.ConnectionTimeoutMS = 0;
  if (OVR_FAILURE(ovr_Initialize(&init_params))) {
    error("failed to initialize OVR");
  }

  if (OVR_FAILURE(ovr_Create(&mHmd, &mGraphicsLuid))) {
    error("failed to create real or debug HMD");
  }
  mHmdDesc = ovr_GetHmdDesc(mHmd);

  // TODO: why would you not want these?
  static const unsigned int supported_tracking_caps =
      ovrTrackingCap_Orientation |
      ovrTrackingCap_MagYawCorrection |
      ovrTrackingCap_Position |
      0;
  static const unsigned int required_tracking_caps = 0;
  if (OVR_FAILURE(ovr_ConfigureTracking(mHmd,
                                        supported_tracking_caps,
                                        required_tracking_caps))) {
    error("failed to configure tracking capabilities");
  }
}

void dk2test::onOculusSDKLogMessage(uintptr_t userData,
                                    int level, const char* message) {
  ((dk2test*)userData)->onOculusSDKLogMessage(level, message);
}

void dk2test::onOculusSDKLogMessage(int level, const char* message) {
  OutputDebugStringA(message);
}

void dk2test::initSDL() {
  SDL_Init(SDL_INIT_VIDEO);

  static const unsigned int flags = SDL_WINDOW_SHOWN;
  int x = SDL_WINDOWPOS_CENTERED;
  int y = SDL_WINDOWPOS_CENTERED;
  mWindow = SDL_CreateWindow(
      "dk2test",
      x, y,
      mHmdDesc.Resolution.w / 2, mHmdDesc.Resolution.h / 2,
      flags);
  if (!mWindow) {
    error("Could not create SDL2 window!");
  }
}

void dk2test::initOgre() {
  // create root
  std::string plugin_filename = "";
  std::string config_filename = "";
  std::string log_filename = "Ogre.log";
  mRoot = new Root(plugin_filename, config_filename, log_filename);

  mRoot->installPlugin(new TheRenderSystemPlugin());
  mRoot->installPlugin(new ParticleFXPlugin());

  // create renderer
  auto rendersystems = mRoot->getAvailableRenderers();
  mRenderSystem = static_cast<TheRenderSystem*>(rendersystems[0]);
  mRenderSystem->setFixedPipelineEnabled(false);
  mRoot->setRenderSystem(mRenderSystem);

  mRoot->initialise(false);

  NameValuePairList params;
  params["vsync"] = "vsync";
  params["gamma"] = "true";
  params["externalWindowHandle"] = StringConverter::toString(
      (size_t)GetNativeWindowHandle());

  mRenderWindow = mRoot->createRenderWindow(
      "OGRE Render Window",
      mHmdDesc.Resolution.w, mHmdDesc.Resolution.h,
      false,
      &params);
  mRenderWindow->setActive(true);
  mRenderWindow->setVisible(true);
  mRenderWindow->setVSyncEnabled(false);
}

void dk2test::loadAssets() {
  // Load resource paths from config file
  ConfigFile config_file;
  config_file.load("resources.cfg");

  // Go through all sections & settings in the file
  ConfigFile::SectionIterator section_iter = config_file.getSectionIterator();

  String group_name, location_type, location_name;
  while (section_iter.hasMoreElements()) {
    group_name = section_iter.peekNextKey();
    ConfigFile::SettingsMultiMap* settings = section_iter.getNext();
    for (auto i = settings->begin(); i != settings->end(); ++i) {
      location_type = i->first;
      location_name = i->second;
      static const bool recursive = false;
      ResourceGroupManager::getSingleton().addResourceLocation(
          location_name, location_type, group_name, recursive);
    }
  }

  ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
}

struct GLTextureHacker : public TheRenderSystemTexture {
  void _freeInternalResourcesImpl() {
    this->freeInternalResourcesImpl();
  }
  void __createSurfaceList() {
    this->_createSurfaceList();
  }
};

static void ShimInOculusTexture(Ogre::Texture* _ogre_texture,
                                ovrTexture* _ovr_texture) {
  const unsigned char * gl_version = glGetString(GL_VERSION);
  int error;

  auto ogre_texture = static_cast<GLTextureHacker*>(_ogre_texture);
  ogre_texture->_freeInternalResourcesImpl();
  glDeleteTextures(1, &ogre_texture->mTextureID);

  ovrGLTexture* ovr_texture = reinterpret_cast<ovrGLTexture*>(_ovr_texture);
  auto texture_id = ovr_texture->OGL.TexId;
  ogre_texture->mTextureID = texture_id;
  auto texture_target = ogre_texture->getGL3PlusTextureTarget();
  auto& gl_support = ogre_texture->mGLSupport;
  glBindTexture(texture_target, texture_id);
  error = glGetError();
  // OculusRoomTiny only uses 1 level of mipmap, does not touch this
  /* static const int num_mipmaps = 0; */
  /* state_man->setTexParameteri( */
  /*     texture_target, GL_TEXTURE_MAX_LEVEL, num_mipmaps); */
  /* error = glGetError(); */
  // copied from OculusRoomTiny demo
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  error = glGetError();
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  error = glGetError();
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  error = glGetError();
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  error = glGetError();
  // only for textures not displayed on HMD
  /* glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, */
  /*              ogre_texture->getWidth(), ogre_texture->getHeight(), 0, */
  /*              GL_RGBA, GL_UNSIGNED_BYTE, 0); */
  /* error = glGetError(); */
  ogre_texture->__createSurfaceList();
}

void dk2test::CreateEyeRenderTargets(float render_quality, float fov_quality) {
  ovrResult ret;
  render_quality = clamp(render_quality, 0.05f, 2.0f);
  fov_quality = clamp(fov_quality, 0.05f, 1.0f);

  for (int i = 0; i < ovrEye_Count; ++i) {
    auto& eye = mEyes[i];
    ovrEyeType eye_type = (ovrEyeType)i;
    eye.RenderQuality = render_quality;
    eye.FovQuality = fov_quality;

    auto& fov = mEyes[i].Fov;
    const auto& max_fov = mHmdDesc.MaxEyeFov[i];
    const auto& default_fov = mHmdDesc.DefaultEyeFov[i];
    fov.UpTan = std::min(eye.FovQuality * default_fov.UpTan, max_fov.UpTan);
    fov.DownTan = std::min(eye.FovQuality * default_fov.DownTan,
                           max_fov.DownTan);
    fov.LeftTan = std::min(eye.FovQuality * default_fov.LeftTan,
                           max_fov.LeftTan);
    fov.RightTan = std::min(eye.FovQuality * default_fov.RightTan,
                            max_fov.RightTan);

    eye.TextureSize = ovr_GetFovTextureSize(mHmd, eye_type,
                                            fov, eye.RenderQuality);
    eye.RenderDesc = ovr_GetRenderDesc(mHmd, eye_type, eye.Fov);

    if (eye.SwapTextureSet) {
      ovr_DestroySwapTextureSet(mHmd, eye.SwapTextureSet);
      eye.SwapTextureSet = nullptr;
    }
    ret = ovr_CreateSwapTextureSetGL(
        mHmd, GL_SRGB8_ALPHA8, eye.TextureSize.w, eye.TextureSize.h,
        &eye.SwapTextureSet);
    if (OVR_FAILURE(ret)) {
      error("failed to create Oculus SDK texture set");
    }

    // create Ogre RenderTextures by shimming in the Oculus supplied textures
    for (int texture_index = 0;
         texture_index < eye.SwapTextureSet->TextureCount;
         ++texture_index) {
      auto& slot = eye.Textures[texture_index];
      // set the OGRE texture's gut to OpenGL invalid 0 texture so we don't do
      // a double free.
      if (slot.Texture.get()) {
        _deleteShimmedOgreTexture(slot.Texture);
      }

      static const int num_mipmaps = 0;
      // MUST BE 0, or mysterious NULL pointer exception, also implied on
      // OculusRoomTiny demo
      static const int multisample = 0;
      // must be true for OGRE to create GL_SRGB8_ALPHA8
      // source: if (hwGamma) in OgreGLPixelFormat.cpp
      static const bool hw_gamma_correction = true;
      // this also must be set to this format for OGRE to create GL_SRGB8_ALPHA8
      static const Ogre::PixelFormat format = Ogre::PF_B8G8R8A8;
      std::string name =
          "EyeRenderTarget"
          + boost::lexical_cast<std::string>(i)
          + "SwapTexture"
          + boost::lexical_cast<std::string>(texture_index);
      slot.Texture = TextureManager::getSingleton().createManual(
          name,
          ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
          TEX_TYPE_2D,
          eye.TextureSize.w, eye.TextureSize.h,
          num_mipmaps,
          format,
          TU_RENDERTARGET,
          nullptr,
          hw_gamma_correction,
          multisample);

      ShimInOculusTexture(slot.Texture.get(),
                          &eye.SwapTextureSet->Textures[texture_index]);
      slot.RenderTarget = slot.Texture->getBuffer()->getRenderTarget();
    }
  }
}

void* dk2test::GetNativeWindowHandle() {
  SDL_SysWMinfo window_info;
  SDL_VERSION(&window_info.version);
  SDL_GetWindowWMInfo(mWindow, &window_info);

#ifdef _WIN32
  return (void*) window_info.info.win.window;
#else
  error("GetNativeWindowHandle() not implemented for this OS")
  return nullptr;
#endif
}

void dk2test::CreateScene() {
  mSceneManager = mRoot->createSceneManager("DefaultSceneManager",
                                            2, INSTANCING_CULLING_SINGLETHREAD);
  mRootNode = mSceneManager->getRootSceneNode();

  // for gross movement by WASD and control sticks
  Ogre::SceneNode* body_node = mRootNode->createChildSceneNode();

  // for finer movement by positional tracking
  Ogre::SceneNode* head_node = body_node->createChildSceneNode();
  mHeadSceneNode = head_node;

  Ogre::Camera* left_eye = mSceneManager->createCamera("LeftEye");
  left_eye->detachFromParent();
  head_node->attachObject(left_eye);
  left_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(left_eye);

  Ogre::Camera* right_eye = mSceneManager->createCamera("RightEye");
  right_eye->detachFromParent();
  head_node->attachObject(right_eye);
  right_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(right_eye);

  body_node->setPosition(Vector3(0, 0, 0.5));
  body_node->_getDerivedPositionUpdated();
  body_node->lookAt(Vector3(0, 0, 0), Node::TS_WORLD);

  // lighting
  mSceneManager->setAmbientLight(ColourValue(0.3f, 0.3f, 0.3f));

  Ogre::SceneNode* node;

  node = mRootNode->createChildSceneNode();
  auto light = mSceneManager->createLight();
  node->attachObject(light);
  light->setType(Ogre::Light::LT_POINT);
  node->setPosition(Ogre::Vector3(0, 0, 1));

  mSceneManager->setSkyBox(true, "Examples/CubeScene");

  node = mRootNode->createChildSceneNode();
  auto ogrehead = mSceneManager->createEntity("ogrehead.mesh");
  node->attachObject(ogrehead);
  node->setPosition(Vector3(0, 0, 0));
  auto scale = 0.005f;
  node->setScale(Vector3(scale, scale, scale));
  node->_getDerivedPositionUpdated();
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);

  node = mRootNode->createChildSceneNode();
  auto sinbad = mSceneManager->createEntity("Sinbad.mesh");
  auto anim_state = sinbad->getAnimationState("Dance");
  mAnimatingStates.push_back(anim_state);
  anim_state->setEnabled(true);
  anim_state->setLoop(true);
  node->attachObject(sinbad);
  scale = 0.05f;
  node->setScale(Vector3(scale, scale, scale));
  node->_getDerivedPositionUpdated();
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);
  node->setPosition(Vector3(-0.25f, 0, 0));

  node = mRootNode->createChildSceneNode();
  auto jaiqua = mSceneManager->createEntity("jaiqua.mesh");
  anim_state = jaiqua->getAnimationState("Sneak");
  mAnimatingStates.push_back(anim_state);
  anim_state->setEnabled(true);
  anim_state->setLoop(true);
  node->attachObject(jaiqua);
  scale = 0.0125f;
  node->setScale(Vector3(scale, scale, scale));
  node->_getDerivedPositionUpdated();
  node->lookAt(Vector3(0, 0, 1), Node::TS_WORLD);
  node->setPosition(Vector3(0.25f, 0, -0.25f));
}

void dk2test::SetupCompositor() {
  auto compositor_manager = mRoot->getCompositorManager2();
  compositor_manager->removeAllWorkspaces();
  if (!compositor_manager->hasWorkspaceDefinition("SwapSet0")) {
    compositor_manager->createBasicWorkspaceDef(
        "SwapSet0", ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
  }
  if (!compositor_manager->hasWorkspaceDefinition("SwapSet1")) {
    compositor_manager->createBasicWorkspaceDef(
        "SwapSet1", ColourValue(0.0f, 0.0f, 0.0f, 1.0f));
  }

  int swap_number;
  CompositorWorkspace* workspace;

  swap_number = 0;
  // left eye, swap texture 0, left camera
  workspace = compositor_manager->addWorkspace(
      mSceneManager,
      mEyes[ovrEye_Left].Textures[swap_number].RenderTarget,
      mEyeCameras[ovrEye_Left],
      "SwapSet0", false, mWorkspaceRenderOrder++);
  mEyes[ovrEye_Left].CompositorWorkspaces[swap_number] = workspace;
  // right eye, swap texture 0, right camera
  workspace = compositor_manager->addWorkspace(
      mSceneManager,
      mEyes[ovrEye_Right].Textures[swap_number].RenderTarget,
      mEyeCameras[ovrEye_Right],
      "SwapSet0", false, mWorkspaceRenderOrder++);
  mEyes[ovrEye_Right].CompositorWorkspaces[swap_number] = workspace;

  swap_number = 1;
  // left eye, swap texture 1, left camera
  workspace = compositor_manager->addWorkspace(
      mSceneManager,
      mEyes[ovrEye_Left].Textures[swap_number].RenderTarget,
      mEyeCameras[ovrEye_Left],
      "SwapSet1", false, mWorkspaceRenderOrder++);
  mEyes[ovrEye_Left].CompositorWorkspaces[swap_number] = workspace;
  // right eye, swap texture 1, right camera
  workspace = compositor_manager->addWorkspace(
      mSceneManager,
      mEyes[ovrEye_Right].Textures[swap_number].RenderTarget,
      mEyeCameras[ovrEye_Right],
      "SwapSet1", false, mWorkspaceRenderOrder++);
  mEyes[ovrEye_Right].CompositorWorkspaces[swap_number] = workspace;
}

void dk2test::SetupMirroring() {
  // should match SDL created window size
  int w = mHmdDesc.Resolution.w / 2;
  int h = mHmdDesc.Resolution.h / 2;

  auto ret = ovr_CreateMirrorTextureGL(
      mHmd, GL_SRGB8_ALPHA8, w, h, &mOvrMirrorTexture);
  if (OVR_FAILURE(ret)) {
    error("Oculus SDK failed to create mirror texture");
  }

  static const int num_mipmaps = 0;
  // MUST BE 0, or mysterious NULL pointer exception, also implied on
  // OculusRoomTiny demo
  static const int multisample = 0;
  // must be true for OGRE to create GL_SRGB8_ALPHA8
  // source: if (hwGamma) in OgreGLPixelFormat.cpp
  static const bool hw_gamma_correction = true;
  // this also must be set to this format for OGRE to create GL_SRGB8_ALPHA8
  static const Ogre::PixelFormat format = Ogre::PF_B8G8R8A8;
  std::string name = "OculusMirrorTexture";
  mMirrorTexture = TextureManager::getSingleton().createManual(
      name,
      ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
      TEX_TYPE_2D,
      w, h,
      num_mipmaps,
      format,
      TU_RENDERTARGET,
      nullptr,
      hw_gamma_correction,
      multisample);
  ShimInOculusTexture(mMirrorTexture.get(), mOvrMirrorTexture);

  mDummySceneManager = mRoot->createSceneManager(
      "DefaultSceneManager", 1, INSTANCING_CULLING_SINGLETHREAD);
  mDummySceneManager->setAmbientLight(ColourValue(1.0f, 1.0f, 1.0f));
  auto root = mDummySceneManager->getRootSceneNode();
  auto camera = mDummySceneManager->createCamera("DummyCamera");
  camera->setPosition(Vector3(0, 0, 0.5f));
  camera->setNearClipDistance(0.1f);
  camera->setFarClipDistance(10000.0f);
  camera->lookAt(Vector3(0, 0, 0));

#if 0
  mDummySceneManager->setSkyBox(true, "Examples/CubeScene", 5000);

  auto node = root->createChildSceneNode();
  auto ogrehead = mDummySceneManager->createEntity("ogrehead.mesh");
  node->attachObject(ogrehead);
  node->setPosition(Vector3(0, 0, 0));
  auto scale = 0.005f;
  node->setScale(Vector3(scale, scale, scale));
  node->_getDerivedPositionUpdated();
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);
#endif

  auto quad = mDummySceneManager->createManualObject();
  root->createChildSceneNode()->attachObject(quad);
  quad->setUseIdentityView(true);
  quad->setUseIdentityProjection(true);

  quad->begin("OculusSdkMirrorMaterial", RenderOperation::OT_TRIANGLE_STRIP);

  // ll
  quad->position(-1.0f, -1.0f, 0.0f);
  quad->textureCoord(0.0f, 2.0f);
  // lr
  quad->position( 3.0f, -1.0f, 0.0f);
  quad->textureCoord(2.0f, 2.0f);
  // ul
  quad->position(-1.0f, 3.0f, 0.0f);
  quad->textureCoord(0.0f, 0.0f);

  quad->index(0);
  quad->index(1);
  quad->index(2);

  quad->end();

  *quad->_getObjectData().mLocalAabb = ArrayAabb::BOX_INFINITE;
  *quad->_getObjectData().mWorldAabb = ArrayAabb::BOX_INFINITE;

  auto compositor_manager = mRoot->getCompositorManager2();
  if (!compositor_manager->hasWorkspaceDefinition("OculusSdkMirror")) {
    compositor_manager->createBasicWorkspaceDef(
        "OculusSdkMirror", ColourValue(0.0f, 1.0f, 0.0f, 1.0f));
  }
  mMirrorWorkspace = compositor_manager->addWorkspace(
      mDummySceneManager,
      mRenderWindow,
      camera,
      "OculusSdkMirror", true);
}

void dk2test::loop() {
  SDL_Event e;
  Timer* timer = new Timer;

  float render_quality = 1.0f;
  float fov = 1.0f;
  bool render_quality_dirty = false;

  while (true) {
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          return;
        case SDL_KEYDOWN: {
          switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_SPACE: {
              ovr_RecenterPose(mHmd);
              //unsigned char rgbColorOut[3];
              //ovrHmd_ProcessLatencyTest(mHmd, rgbColorOut);
              break;
            }
            case SDL_SCANCODE_W: {
              render_quality += 0.1f;
              render_quality_dirty = true;
              break;
            }
            case SDL_SCANCODE_S: {
              render_quality -= 0.1f;
              render_quality_dirty = true;
              break;
            }
            case SDL_SCANCODE_A: {
              fov -= 0.1f;
              render_quality_dirty = true;
              break;
            }
            case SDL_SCANCODE_D: {
              fov += 0.1f;
              render_quality_dirty = true;
              break;
            }
          }
          break;
        }
      }

      if (render_quality_dirty) {
        render_quality_dirty = false;
        this->CreateEyeRenderTargets(render_quality, fov);
        this->SetupCompositor();
      }
    }

    double us = timer->getMicroseconds() / 1e6;
    timer->reset();
    for (auto anim_state : mAnimatingStates) {
      anim_state->addTime((float)us);
    }

    this->renderOculusFrame();
  }
}

static Ogre::Matrix4& ToOgreMatrix(
    const ovrMatrix4f& src, Ogre::Matrix4& dst)
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      dst[i][j] = src.M[i][j];
    }
  }

  return dst;
}

void dk2test::renderOculusFrame() {
  using namespace Ogre;

  // this is the primary 3D layer, later on we might want to lower the quality
  // of this for performance and introduce other high quality layers, such as
  // text.
  ovrLayerEyeFov primary_layer;
  primary_layer.Header.Type = ovrLayerType_EyeFov;
  primary_layer.Header.Flags =
      ovrLayerFlag_HighQuality |
      /* ovrLayerFlag_TextureOriginAtBottomLeft | */
      0;

  auto frame_timing = ovr_GetFrameTiming(mHmd, 0);
  auto ts = ovr_GetTrackingState(mHmd, frame_timing.DisplayMidpointSeconds);
  const auto& head_pose = ts.HeadPose.ThePose;

  ovrEyeRenderDesc eye_render_descs[2];
  ovrVector3f hmd_to_eye_view_offsets[2];
  for (int i = 0; i < ovrEye_Count; ++i) {
    eye_render_descs[i] = ovr_GetRenderDesc(
        mHmd, (ovrEyeType)i, mEyes[i].Fov);
    hmd_to_eye_view_offsets[i] = eye_render_descs[i].HmdToEyeViewOffset;
  }
  ovrPosef eye_poses[2];
  ovr_CalcEyePoses(head_pose, hmd_to_eye_view_offsets, eye_poses);

  for (int i = 0; i < ovrEye_Count; ++i) {
    auto& eye = mEyes[(ovrEyeType)i];
    const auto& render_desc = eye_render_descs[i];
    const auto& hmd_to_eye_view_offset = hmd_to_eye_view_offsets[i];
    const auto& eye_pose = eye_poses[i];

    const auto& orient = eye_pose.Orientation;
    Quaternion orientation(orient.w, orient.x, orient.y, orient.z);
    const auto& pos = eye_pose.Position;
    Vector3 position(pos.x, pos.y, pos.z);

    mHeadSceneNode->setPosition(position);

    unsigned int projection_modifiers =
        /* ovrProjection_None | */
        ovrProjection_RightHanded |
        /* ovrProjection_FarLessThanNear | */
        /* ovrProjection_FarClipAtInfinity | */
        /* ovrProjection_ClipRangeOpenGL | */
        0;
    ovrMatrix4f projection = ovrMatrix4f_Projection(
        eye.Fov, 0.01f, 10000.0f, projection_modifiers);

    auto camera = mEyeCameras[i];
    Vector3 world_eye_pos = camera->getDerivedPosition();
    Matrix4 view;
    view.makeTrans(-world_eye_pos);
    Matrix4 orientation_transform(orientation.Inverse());
    view = orientation_transform * view;
    Matrix4 eye_offset_translate;
    const auto& eye_off = hmd_to_eye_view_offset;
    Vector3 eye_offset(eye_off.x, eye_off.y, eye_off.z);
    eye_offset_translate.makeTrans(-eye_offset);
    view = eye_offset_translate * view;

    camera->setCustomViewMatrix(true, view);
    Matrix4 m;
    camera->setCustomProjectionMatrix(true, ToOgreMatrix(projection, m));

    int count = eye.SwapTextureSet->TextureCount;
    auto& current_index = eye.SwapTextureSet->CurrentIndex;
    current_index = (current_index + 1) % count;
    int other_index = (current_index + 1) % count;

    auto& slot = eye.Textures[current_index];
    auto& other_slot = eye.Textures[other_index];

    eye.CompositorWorkspaces[current_index]->setEnabled(true);
    eye.CompositorWorkspaces[other_index]->setEnabled(false);

    primary_layer.ColorTexture[i] = eye.SwapTextureSet;
    primary_layer.Viewport[i].Pos = {0, 0};
    primary_layer.Viewport[i].Size = {
      (int)slot.Texture->getWidth(), (int)slot.Texture->getHeight()
    };
    primary_layer.Fov[i] = eye.Fov;
    primary_layer.RenderPose[i] = eye_pose;
  }

  mMirrorWorkspace->setEnabled(true);
  mRoot->renderOneFrame();

  ovrLayerHeader* layers[1] = {&primary_layer.Header};
  auto result = ovr_SubmitFrame(mHmd, 0, nullptr, layers, 1);

  mEyes[0].CompositorWorkspaces[0]->setEnabled(false);
  mEyes[0].CompositorWorkspaces[1]->setEnabled(false);
  mEyes[1].CompositorWorkspaces[0]->setEnabled(false);
  mEyes[1].CompositorWorkspaces[1]->setEnabled(false);
  mMirrorWorkspace->setEnabled(true);
  mRoot->renderOneFrame();
}
