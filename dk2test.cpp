#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

using namespace Ogre;

dk2test::dk2test() 
    : mHmd(nullptr)
{
  memset(&mGraphicsLuid, 0, sizeof(mGraphicsLuid));
  memset(&mHmdDesc, 0, sizeof(mHmdDesc));
  
  mEyes[ovrEye_Left].SwapTextureSet = nullptr;
  mEyes[ovrEye_Right].SwapTextureSet = nullptr;
  
  this->initOVR();
  this->initSDL();
  this->initOgre();
  this->loadAssets();
  this->createRenderTextureViewer();
}

dk2test::~dk2test() {
  for (int i = 0; i < 2; ++i) {
    mEyes[0].Textures[i].Texture.setNull();
    mEyes[1].Textures[i].Texture.setNull();
  }
  delete mRoot;
  mRoot = NULL;

  SDL_DestroyWindow(mWindow);
  SDL_Quit();

  ovr_Destroy(mHmd);
  mHmd = NULL;
  ovr_Shutdown();
}

void dk2test::initOVR() {
  ovrInitParams init_params;
  
  init_params.Flags = 0;
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
      ovrTrackingCap_Idle |
      0;
  static const unsigned int required_tracking_caps = 0;
  if (OVR_FAILURE(ovr_ConfigureTracking(mHmd, supported_tracking_caps, required_tracking_caps))) {
    error("failed to configure tracking capabilities");
  }
}

void dk2test::onOculusSDKLogMessage(uintptr_t userData, int level, const char* message) {
  ((dk2test*)userData)->onOculusSDKLogMessage(level, message);
}

void dk2test::onOculusSDKLogMessage(int level, const char* message) {
  /* auto& log_man = LogManager::getSingleton(); */
  /* log_man.getDefaultLog()->logMessage(message); */
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

  // load dynamic libraries
  /*
  std::vector<std::string> dlls = {
    "RenderSystem_GL",
    "Plugin_OctreeSceneManager",
    "Plugin_ParticleFX",
  };

  for (auto dll : dlls) {
#ifdef _DEBUG
    dll += "_d";
#endif
    mRoot->loadPlugin(dll);
  }
  */
  mRoot->installPlugin(new GLPlugin());
  mRoot->installPlugin(new OctreePlugin());
  mRoot->installPlugin(new ParticleFXPlugin());

  // create renderer
  auto rendersystems = mRoot->getAvailableRenderers();
  mGlRenderSystem = static_cast<GLRenderSystem*>(rendersystems[0]);
  mGlRenderSystem->setFixedPipelineEnabled(false);
  mRoot->setRenderSystem(mGlRenderSystem);

  mRoot->initialise(false);

  NameValuePairList params;
  params["vsync"] = "vsync";
  params["externalWindowHandle"] = StringConverter::toString((size_t)GetNativeWindowHandle());

  mRenderWindow = mRoot->createRenderWindow(
      "OGRE Render Window",
      mHmdDesc.Resolution.w, mHmdDesc.Resolution.h,
      false,
      &params);
  mRenderWindow->setActive(true);
  mRenderWindow->setVisible(true);
  mRenderWindow->setAutoUpdated(false);
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

struct GLTextureHacker : public GLTexture {
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
  auto gl_texture_target = ogre_texture->getGLTextureTarget();
  auto& gl_support = ogre_texture->mGLSupport;
  auto state_man = gl_support.getStateCacheManager();
  state_man->bindGLTexture(gl_texture_target, texture_id);
  error = glGetError();
  // OculusRoomTiny only uses 1 level of mipmap, does not touch this
  /* static const int num_mipmaps = 0; */
  /* state_man->setTexParameteri( */
  /*     gl_texture_target, GL_TEXTURE_MAX_LEVEL, num_mipmaps); */
  /* error = glGetError(); */
  // copied from OculusRoomTiny demo
  state_man->setTexParameteri(
      gl_texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  error = glGetError();
  state_man->setTexParameteri(
      gl_texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  error = glGetError();
  state_man->setTexParameteri(
      gl_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  error = glGetError();
  state_man->setTexParameteri(
      gl_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  error = glGetError();
  // only for textures not displayed on HMD
  /* glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, */
  /*              ogre_texture->getWidth(), ogre_texture->getHeight(), 0, */ 
  /*              GL_RGBA, GL_UNSIGNED_BYTE, 0); */
  /* error = glGetError(); */
  ogre_texture->__createSurfaceList();
}

void dk2test::ConfigureRenderingQuality(float render_quality, float fov_quality) {
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
      eye.SwapTextureSet = NULL;
    }
    ret = ovr_CreateSwapTextureSetGL(
        mHmd, GL_SRGB8_ALPHA8, eye.TextureSize.w, eye.TextureSize.h,
        &eye.SwapTextureSet);
    if (OVR_FAILURE(ret)) {
      error("failed to create Oculus SDK texture set");
    }
    
    for (int texture_index = 0;
         texture_index < eye.SwapTextureSet->TextureCount;
         ++texture_index) {
      auto& slot = eye.Textures[texture_index];
      // set the OGRE texture's gut to OpenGL invalid 0 texture so we don't do
      // a double free.
      if (slot.Texture.get()) {
        auto texture = static_cast<Ogre::GLTexture*>(slot.Texture.get());
        // HACK BEGIN
        texture->mSurfaceList.clear();
        texture->mGLSupport.getStateCacheManager()->invalidateStateForTexture(
            texture->mTextureID);
        texture->mTextureID = 0;
        // HACK END
        std::string texture_name(slot.Texture->getName());
        slot.Texture.setNull();
        Ogre::TextureManager::getSingleton().remove(texture_name);
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
          NULL,
          hw_gamma_correction,
          multisample);
      
      ShimInOculusTexture(slot.Texture.get(),
                          &eye.SwapTextureSet->Textures[texture_index]);
      slot.RenderTarget = slot.Texture->getBuffer()->getRenderTarget();
      slot.RenderTarget->setAutoUpdated(false);
    }
    auto render_target =
        eye.Textures[0].Texture->getBuffer()->getRenderTarget();
    eye.DepthBuffer = std::unique_ptr<DepthBuffer>(
        mGlRenderSystem->_createDepthBufferFor(render_target));
  }
}

void* dk2test::GetNativeWindowHandle() {
  SDL_SysWMinfo window_info;
  SDL_VERSION(&window_info.version);
  SDL_GetWindowWMInfo(mWindow, &window_info);

#ifdef _WIN32
  return (void*) window_info.info.win.window;
#endif
}

void dk2test::CreateScene() {
  mSceneManager = mRoot->createSceneManager(Ogre::ST_GENERIC);
  mRootNode = mSceneManager->getRootSceneNode();

  /* mSceneManager->setSkyBox(true, "Examples/EveningSkyBox"); */
  mSceneManager->setAmbientLight(ColourValue(0.333f, 0.333f, 0.333f));

  // for gross movement by WASD and control sticks
  Ogre::SceneNode* body_node = mRootNode->createChildSceneNode("BodyNode");

  // for finer movement by positional tracking
  Ogre::SceneNode* head_node = body_node->createChildSceneNode("HeadNode");
  mHeadSceneNode = head_node;

  Ogre::Camera* left_eye = mSceneManager->createCamera("LeftEye");
  head_node->attachObject(left_eye);
  left_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(left_eye);

  Ogre::Camera* right_eye = mSceneManager->createCamera("RightEye");
  head_node->attachObject(right_eye);
  right_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(right_eye);

  body_node->setPosition(Vector3(0, 0, 0.5));
  body_node->lookAt(Vector3(0, 0, 0), Node::TS_WORLD);

  auto node = mRootNode->createChildSceneNode("ogrehead");
  /* auto ogrehead = mSceneManager->createEntity("ogrehead.mesh"); */
  /* node->attachObject(ogrehead); */
  /* node->setPosition(Vector3(0, 0, 0)); */
  auto scale = 0.005f;
  /* node->setScale(Vector3(scale, scale, scale)); */
  /* node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD); */

  auto light = mSceneManager->createLight();
  node->attachObject(light);
  light->setType(Ogre::Light::LT_POINT);
  light->setPosition(Ogre::Vector3(0, 0, 1));

  /* node = mRootNode->createChildSceneNode("sinbad"); */
  /* auto sinbad = mSceneManager->createEntity("Sinbad.mesh"); */
  /* auto anim_state = sinbad->getAnimationState("Dance"); */
  /* mAnimatingStates.push_back(anim_state); */
  /* anim_state->setEnabled(true); */
  /* anim_state->setLoop(true); */
  /* node->attachObject(sinbad); */
  /* scale = 0.05f; */
  /* node->setScale(Vector3(scale, scale, scale)); */
  /* node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD); */
  /* node->setPosition(Vector3(-0.25f, 0, 0)); */

  /* node = mRootNode->createChildSceneNode("jaiqua"); */
  /* auto jaiqua = mSceneManager->createEntity("jaiqua.mesh"); */
  /* anim_state = jaiqua->getAnimationState("Sneak"); */
  /* mAnimatingStates.push_back(anim_state); */
  /* anim_state->setEnabled(true); */
  /* anim_state->setLoop(true); */
  /* node->attachObject(jaiqua); */
  /* scale = 0.0125f; */
  /* node->setScale(Vector3(scale, scale, scale)); */
  /* node->lookAt(Vector3(0, 0, 1), Node::TS_WORLD); */
  /* node->setPosition(Vector3(0.25f, 0, -0.25f)); */
}

void dk2test::AttachSceneToRenderTargets() {
#if 0
  for (int i = 0; i < ovrEye_Count; ++i) {
    auto& eye = mEyes[i];
    auto camera = mEyeCameras[i];
    for (int texture_index = 0;
         texture_index < eye.SwapTextureSet->TextureCount;
         ++texture_index) {
      auto& slot = eye.Textures[texture_index];
      auto render_target = slot.Texture->getBuffer()->getRenderTarget();
      slot.RenderTarget = render_target;
      static const int z_order = 0;
      static const float left = 0.0f, top = 0.0f, width = 1.0f, height = 1.0f;
      render_target->addViewport(camera, z_order, left, top, width, height);
      render_target->setAutoUpdated(false);
    }
  }
#endif
}

void dk2test::createRenderTextureViewer() {
  return;
  mDummySceneManager = mRoot->createSceneManager(ST_GENERIC);
  mDummySceneManager->setSkyBox(true, "Examples/EveningSkyBox");
  auto root_node = mDummySceneManager->getRootSceneNode();
  auto node = root_node->createChildSceneNode("Background");
  auto dummy_camera = mDummySceneManager->createCamera("dummy0");
  node->attachObject(dummy_camera);
  
#if 0
  auto rect = new Rectangle2D(true);
  rect->setCorners(-1, 1, 1, -1);

  MaterialPtr material = MaterialManager::getSingleton().create(
      "VR_Quad", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
  material->getTechnique(0)->getPass(0)->createTextureUnitState("EyeRenderTarget");
  material->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
  material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
  material->getTechnique(0)->getPass(0)->setLightingEnabled(false);

  rect->setMaterial("VR_Quad");
  rect->setBoundingBox(AxisAlignedBox::BOX_INFINITE);

  node->attachObject(rect);
#endif

  auto viewport = mRenderWindow->addViewport(dummy_camera);
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
        this->ConfigureRenderingQuality(render_quality, fov);
        this->AttachSceneToRenderTargets();
      }
    }

    double us = timer->getMicroseconds() / 1e6;
    timer->reset();

    for (auto anim_state : mAnimatingStates) {
      anim_state->addTime((float)us);
    }

    // this updates animations
    mRoot->renderOneFrame();
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
  glGetString(GL_VERSION);
  using namespace Ogre;
  
  // this is the primary 3D layer, later on we might want to lower the quality
  // of this for performance and introduce other high quality layers, such as
  // text.
  ovrLayerEyeFov primary_layer;
  primary_layer.Header.Type = ovrLayerType_EyeFov;
  primary_layer.Header.Flags = 
      /* ovrLayerFlag_HighQuality | */
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

    const auto& orient = head_pose.Orientation;
    Quaternion orientation(orient.w, orient.x, orient.y, orient.z);
    const auto& pos = head_pose.Position;
    Vector3 position(pos.x, pos.y, pos.z);

    // right now this is redundantly set twice (once per eye), but in the older
    // SDK it was actually NOT redundant, and you would get the new head pose
    // after rendering one eye. keeping this structure in case this bcomes true
    // again.
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
    
    // at every update loop, the we take turns using the target texture to
    // render. my guess is that we can't render onto a texture that is being
    // used by the SDK as that is introducing undesirable blocking behavior.
    eye.SwapTextureSet->CurrentIndex =
        (eye.SwapTextureSet->CurrentIndex + 1)
        % eye.SwapTextureSet->TextureCount;
    
    auto& slot = eye.Textures[eye.SwapTextureSet->CurrentIndex];
    
    primary_layer.ColorTexture[i] = eye.SwapTextureSet;
    primary_layer.Viewport[i].Pos = {0, 0};
    primary_layer.Viewport[i].Size = {
      (int)slot.Texture->getWidth(), (int)slot.Texture->getHeight()
    };
    primary_layer.Fov[i] = eye.Fov;
    primary_layer.RenderPose[i] = eye_pose;
    
    auto render_target = slot.RenderTarget;
    render_target->setAutoUpdated(false);
    render_target->attachDepthBuffer(eye.DepthBuffer.get());
    static const int z_order = 0;
    static const float left = 0.0f, top = 0.0f, width = 1.0f, height = 1.0f;
    Viewport* viewport =
        render_target->addViewport(camera, z_order, left, top, width, height);
    viewport->setClearEveryFrame(false);
    render_target->update();
    
    render_target->removeAllViewports();
    render_target->detachDepthBuffer();
  }
  
  ovrLayerHeader* layers[1] = {&primary_layer.Header};
  auto result = ovr_SubmitFrame(mHmd, 0, nullptr, layers, 1);
  
  /* mRenderWindow->update(); */

  //std::string test_result(ovrHmd_GetLatencyTestResult(mHmd));
  //if (test_result.size() > 0) {
    //OutputDebugStringA(test_result.c_str());
    //OutputDebugStringA("\n");
  //}
}
