#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

using namespace Ogre;

dk2test::dk2test() 
    : mUsingDebugHmd(false),
      mHmd(NULL) {
  this->initOVR();
  this->initSDL();
  this->initOgre();
  this->loadAssets();
}

dk2test::~dk2test() {
  mEyes[0].Texture.setNull();
  mEyes[1].Texture.setNull();
  delete mRoot;
  mRoot = NULL;

  SDL_DestroyWindow(mWindow);
  SDL_Quit();

  ovrHmd_Destroy(mHmd);
  mHmd = NULL;
  ovr_Shutdown();
}

void dk2test::initOVR() {
  ovr_InitializeRenderingShim();

  if (!ovr_Initialize()) {
    error("failed to initialize OVR");
  }

  mHmd = ovrHmd_Create(0);
  if (!mHmd) {
    mHmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    mUsingDebugHmd = true;
  }
  if (!mHmd) {
    error("failed to create real or debug HMD");
  }

  // TODO: why would you not want these?
  static const unsigned int supported_tracking_caps = 
      ovrTrackingCap_Orientation |
      ovrTrackingCap_MagYawCorrection |
      ovrTrackingCap_Position;
  static const unsigned int required_tracking_caps = 0;
  if (!ovrHmd_ConfigureTracking(mHmd, supported_tracking_caps, required_tracking_caps)) {
    error("failed to configure tracking capabilities");
  }

  // TODO: why would you not want these?
  static const unsigned int hmd_caps =
      ovrHmdCap_LowPersistence |
      ovrHmdCap_DynamicPrediction;
  ovrHmd_SetEnabledCaps(mHmd, hmd_caps);
}

void dk2test::initSDL() {
  SDL_Init(SDL_INIT_VIDEO);

  static const unsigned int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;
  int x = SDL_WINDOWPOS_CENTERED;
  int y = SDL_WINDOWPOS_CENTERED;
  if (!mUsingDebugHmd) {
    x = mHmd->WindowsPos.x;
    y = mHmd->WindowsPos.y;
  }
  mWindow = SDL_CreateWindow(
      "dk2test",
      x, y,
      this->mHmd->Resolution.w, this->mHmd->Resolution.h,
      flags);
  if (!mWindow) {
    error("Could not create SDL2 window!");
  }

  // TODO: this always seems to be true in SDK 0.4.1
  bool need_attach = !(ovrHmd_GetEnabledCaps(mHmd) & ovrHmdCap_ExtendDesktop);
  if (need_attach)
    ovrHmd_AttachToWindow(mHmd, this->GetNativeWindowHandle(), NULL, NULL);
}

void dk2test::initOgre() {
  // create root
  std::string plugin_filename = "";
  std::string config_filename = "";
  std::string log_filename = "Ogre.log";
  mRoot = new Root(plugin_filename, config_filename, log_filename);

  // load dynamic libraries
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

  // create renderer
  auto rendersystems = mRoot->getAvailableRenderers();
  mRoot->setRenderSystem(rendersystems[0]);

  mRoot->initialise(false);

  NameValuePairList params;
  params["externalWindowHandle"] = StringConverter::toString((size_t)GetNativeWindowHandle());

  mRenderWindow = mRoot->createRenderWindow(
      "OGRE Render Window",
      this->mHmd->Resolution.w, this->mHmd->Resolution.h,
      false,
      &params);
  mRenderWindow->setActive(true);
  mRenderWindow->setVisible(true);
  // we will manually render to eye textures later on, and the Oculus SDK will swap buffers
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

void dk2test::ConfigureRenderingQuality(float render_quality, float fov_quality) {
  render_quality = clamp(render_quality, 0.1f, 1.0f);
  fov_quality = clamp(fov_quality, 0.1f, 1.0f);

  for (int i = 0; i < ovrEye_Count; ++i) {
    auto& eye = mEyes[i];
    eye.RenderQuality = render_quality;
    eye.FovQuality = fov_quality;

    auto& fov = mEyes[i].Fov;
    const auto& max_fov = mHmd->MaxEyeFov[i];
    const auto& default_fov = mHmd->DefaultEyeFov[i];
    fov.UpTan = std::min(eye.FovQuality * default_fov.UpTan, max_fov.UpTan);
    fov.DownTan = std::min(eye.FovQuality * default_fov.DownTan, max_fov.DownTan);
    fov.LeftTan = std::min(eye.FovQuality * default_fov.LeftTan, max_fov.LeftTan);
    fov.RightTan = std::min(eye.FovQuality * default_fov.RightTan, max_fov.RightTan);

    eye.TextureSize = ovrHmd_GetFovTextureSize(mHmd, (ovrEyeType)i, fov, eye.RenderQuality);

    if (eye.Texture.get()) {
      std::string texture_name(eye.Texture->getName());
      eye.Texture.setNull();
      Ogre::TextureManager::getSingleton().remove(texture_name);
    }

    static const int num_mipmaps = 0;
    static const bool hw_gamma_correction = false;
    static const int multisample = 4;
    eye.Texture = TextureManager::getSingleton().createManual(
        "EyeRenderTarget" + boost::lexical_cast<std::string>(i),
        ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        TEX_TYPE_2D,
        eye.TextureSize.w, eye.TextureSize.h,
        num_mipmaps,
        PF_R8G8B8,
        TU_RENDERTARGET,
        NULL,
        hw_gamma_correction,
        multisample);
    eye.Texture->getCustomAttribute("GLID", &eye.TextureId);

    // might have changed due to hardware limitations
    eye.TextureSize.w = eye.Texture->getWidth();
    eye.TextureSize.h = eye.Texture->getHeight();
  }

  ovrGLConfig config;
  config.OGL.Header.API = ovrRenderAPI_OpenGL;
  config.OGL.Header.RTSize = OVR::Sizei(mHmd->Resolution.w, mHmd->Resolution.h);
  // this does not appear to be used as of SDK 0.4.1?
  config.OGL.Header.Multisample = 0;
  config.OGL.Window = (decltype(config.OGL.Window)) this->GetNativeWindowHandle();
  config.OGL.DC = NULL;

  int distortion_caps =
      ovrDistortionCap_Chromatic |
      ovrDistortionCap_Vignette |
      ovrDistortionCap_TimeWarp |
      ovrDistortionCap_Overdrive |
      // needed since OGRE does flipping to compensate for OpenGL texture coordinate system
      ovrDistortionCap_FlipInput;
  ovrFovPort fovs[2] = { mEyes[0].Fov, mEyes[1].Fov };
  ovrEyeRenderDesc render_descs[2];
  ovrHmd_ConfigureRendering(
      mHmd, &config.Config, distortion_caps,
      fovs, render_descs);
  mEyes[0].RenderDesc = render_descs[0];
  mEyes[1].RenderDesc = render_descs[1];

  ovrhmd_EnableHSWDisplaySDKRender(mHmd, false);
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

  mSceneManager->setSkyBox(true, "Examples/EveningSkyBox");
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
  auto ogrehead = mSceneManager->createEntity("ogrehead.mesh");
  node->attachObject(ogrehead);
  node->setPosition(Vector3(0, 0, 0));
  auto scale = 0.005f;
  node->setScale(Vector3(scale, scale, scale));
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);

  auto light = mSceneManager->createLight();
  node->attachObject(light);
  light->setType(Ogre::Light::LT_POINT);
  light->setPosition(Ogre::Vector3(0, 0, 1));

  node = mRootNode->createChildSceneNode("sinbad");
  auto sinbad = mSceneManager->createEntity("Sinbad.mesh");
  auto anim_state = sinbad->getAnimationState("Dance");
  mAnimatingStates.push_back(anim_state);
  anim_state->setEnabled(true);
  anim_state->setLoop(true);
  node->attachObject(sinbad);
  scale = 0.05f;
  node->setScale(Vector3(scale, scale, scale));
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);
  node->setPosition(Vector3(-0.25f, 0, 0));

  node = mRootNode->createChildSceneNode("jaiqua");
  auto jaiqua = mSceneManager->createEntity("jaiqua.mesh");
  anim_state = jaiqua->getAnimationState("Sneak");
  mAnimatingStates.push_back(anim_state);
  anim_state->setEnabled(true);
  anim_state->setLoop(true);
  node->attachObject(jaiqua);
  scale = 0.0125f;
  node->setScale(Vector3(scale, scale, scale));
  node->lookAt(Vector3(0, 0, 1), Node::TS_WORLD);
  node->setPosition(Vector3(0.25f, 0, -0.25f));
}

void dk2test::AttachSceneToRenderTargets() {
  for (int i = 0; i < ovrEye_Count; ++i) {
    auto& eye = mEyes[i];
    auto camera = mEyeCameras[i];
    auto render_target = eye.Texture->getBuffer()->getRenderTarget();
    eye.RenderTarget = render_target;
    static const int z_order = 0;
    static const float left = 0.0f, top = 0.0f, width = 1.0f, height = 1.0f;
    render_target->addViewport(camera, z_order, left, top, width, height);
    render_target->setAutoUpdated(false);
  }
}

#if 0
void dk2test::createRenderTextureViewer() {
  mDummySceneManager = mRoot->createSceneManager(ST_GENERIC);
  mDummySceneManager->setSkyBox(true, "Examples/EveningSkyBox");
  auto root_node = mDummySceneManager->getRootSceneNode();
  auto node = root_node->createChildSceneNode("Background");
  auto dummy_camera = mDummySceneManager->createCamera("dummy0");
  node->attachObject(dummy_camera);

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

  auto viewport = mRenderWindow->addViewport(dummy_camera);
}
#endif

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
              ovrHmd_RecenterPose(mHmd);
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

    mRoot->renderOneFrame();
    this->renderOculusFrame();
  }
}

static inline const Ogre::Matrix4& ToOgreMatrix(const OVR::Matrix4f& src, Ogre::Matrix4& dst) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      dst[i][j] = src.M[i][j];
    }
  }

  return dst;
}

void dk2test::renderOculusFrame() {
  using namespace OVR;

  ovrFrameTiming hmd_frame_timing = ovrHmd_BeginFrame(mHmd, 0);
  ovrPosef head_pose[2];

  Ogre::Matrix4 m;

  for (int counter = 0; counter < ovrEye_Count; ++counter) {
    // in the DK2 right eye is actually first!
    ovrEyeType eye_index = mHmd->EyeRenderOrder[counter];
    auto& eye = mEyes[eye_index];
    head_pose[eye_index] = ovrHmd_GetEyePose(mHmd, eye_index);

    Quatf orientation = Quatf(head_pose[eye_index].Orientation);
    Matrix4f proj = ovrMatrix4f_Projection(eye.RenderDesc.Fov, 0.01f, 10000.0f, true);

    auto camera = mEyeCameras[eye_index];
    auto pos = camera->getDerivedPosition();
    Vector3f world_eye_pos(pos.x, pos.y, pos.z);
    Matrix4f view = (Matrix4f(orientation.Inverted()) * Matrix4f::Translation(-world_eye_pos));
    view = Matrix4f::Translation(eye.RenderDesc.ViewAdjust) * view;

    camera->setCustomViewMatrix(true, ToOgreMatrix(view, m));
    camera->setCustomProjectionMatrix(true, ToOgreMatrix(proj, m));

    auto head_pos = head_pose[eye_index].Position;
    mHeadSceneNode->setPosition(Ogre::Vector3(head_pos.x, head_pos.y, head_pos.z));

    eye.RenderTarget->update();
  }

  ovrGLTexture gl_eye_textures[2];
  ovrTexture eye_textures[2];
  for (int i = 0; i < ovrEye_Count; ++i ) {
    auto& eye = mEyes[i];

    gl_eye_textures[i].OGL.Header.API = ovrRenderAPI_OpenGL;
    gl_eye_textures[i].OGL.Header.TextureSize = eye.TextureSize;
    gl_eye_textures[i].OGL.TexId = eye.TextureId;

    auto& rect = gl_eye_textures[i].OGL.Header.RenderViewport;
    rect.Size = eye.TextureSize;
    rect.Pos.x = 0;
    rect.Pos.y = 0;
    eye_textures[i] = gl_eye_textures[i].Texture;
  }

  ovrHmd_EndFrame(mHmd, head_pose, eye_textures);

  //std::string test_result(ovrHmd_GetLatencyTestResult(mHmd));
  //if (test_result.size() > 0) {
    //OutputDebugStringA(test_result.c_str());
    //OutputDebugStringA("\n");
  //}
}
