#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

using namespace Ogre;

dk2test::dk2test() {
  this->initOVR();
  this->initSDL();
  this->initOgre();
  this->initOVR2();

  this->createScene();
  //this->createRenderTextureViewer();
}

dk2test::~dk2test() {
  this->destroySDL();
  //this->destroyOVR();
}

void dk2test::initOVR() {
  ovr_Initialize();

  mHmd = ovrHmd_Create(0);
  if (!mHmd) {
    //notice("Could not find a real Oculus head mounted display! Creating a debug one based on the DK1.");
    mHmd = ovrHmd_CreateDebug(ovrHmd_DK2);
  }

  mRecommendedTexSize[kLeft] = ovrHmd_GetFovTextureSize(
      mHmd, ovrEye_Left, mHmd->DefaultEyeFov[0], 1.0f);
  mRecommendedTexSize[kRight] = ovrHmd_GetFovTextureSize(
      mHmd, ovrEye_Right, mHmd->DefaultEyeFov[1], 1.0f);

  mRenderTargetSize.w = mRecommendedTexSize[kLeft].w + mRecommendedTexSize[kRight].w;
  mRenderTargetSize.h = std::max(mRecommendedTexSize[kLeft].h, mRecommendedTexSize[kRight].h);

  static const unsigned int supportedTrackingCaps = 
      ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;
  static const unsigned int requiredTrackingCaps = 0;
  ovrHmd_ConfigureTracking(mHmd, supportedTrackingCaps, requiredTrackingCaps);
}

void dk2test::initSDL() {
  SDL_Init(SDL_INIT_VIDEO);

  static const unsigned int flags =
      SDL_WINDOW_SHOWN |
      SDL_WINDOW_BORDERLESS |
      0;
  mWindow = SDL_CreateWindow(
      "dk2test",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      this->mHmd->Resolution.w, this->mHmd->Resolution.h,
      flags
      );
  if (!mWindow) {
    error("Could not create SDL2 window!");
  }
}

void dk2test::initOgre() {
  // create root
  std::string plugin_filename = "";
  std::string config_filename = "";
  std::string log_filename = "Ogre.log";
  mRoot = new Ogre::Root(plugin_filename, config_filename, log_filename);

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

  Ogre::NameValuePairList params;
  params["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)getNativeWindowHandle());

  mRenderWindow = mRoot->createRenderWindow(
      "OGRE Render Window",
      this->mHmd->Resolution.w, this->mHmd->Resolution.h,
      false,
      &params);
  mRenderWindow->setVisible(true);
  mRenderWindow->setAutoUpdated(false);
  mRenderWindow->setActive(true);

  rendersystems[0]->clearFrameBuffer(Ogre::FBT_COLOUR, Ogre::ColourValue::Red);
  mRoot->renderOneFrame(0.0f);

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

void dk2test::initOVR2() {
  static const int num_mipmaps = 0;
  static const bool hw_gamma_correction = false;
  mEyeRenderMultisample = 0;

  mEyeRenderTexture = TextureManager::getSingleton().createManual(
      "EyeRenderTarget",
      ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
      TEX_TYPE_2D,
      mRenderTargetSize.w, mRenderTargetSize.h,
      num_mipmaps,
      PF_R8G8B8,
      TU_RENDERTARGET,
      NULL,
      hw_gamma_correction,
      mEyeRenderMultisample);
  mEyeRenderTexture->getCustomAttribute("GLID", &mEyeRenderTargetTextureId);

  // might have changed due to hardware limitations
  mRenderTargetSize.w = mEyeRenderTexture->getWidth();
  mRenderTargetSize.h = mEyeRenderTexture->getHeight();

  ovrGLConfig cfg;
  cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
  cfg.OGL.Header.RTSize = OVR::Sizei(mHmd->Resolution.w, mHmd->Resolution.h);
  cfg.OGL.Header.Multisample = mEyeRenderMultisample;
  cfg.OGL.Window = (decltype(cfg.OGL.Window)) this->getNativeWindowHandle();
  cfg.OGL.DC = NULL;

  int distortionCaps =
      ovrDistortionCap_Chromatic |
      ovrDistortionCap_Vignette |
      ovrDistortionCap_TimeWarp |
      ovrDistortionCap_Overdrive |
      // needed since OGRE does flipping to compensate for OpenGL texture coordinate system
      ovrDistortionCap_FlipInput |
      0;

  ovrBool result = ovrHmd_ConfigureRendering(
      mHmd, &cfg.Config, distortionCaps,
      mHmd->DefaultEyeFov, mEyeRenderDesc);

  ovrHmd_SetEnabledCaps(mHmd, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);
}

void dk2test::destroyOgre() {
  delete mRoot;
  mRoot = NULL;
}

void dk2test::destroySDL() {
  SDL_DestroyWindow(mWindow);
  SDL_Quit();
}

void dk2test::destroyOVR() {
  ovrHmd_Destroy(mHmd);
  mHmd = NULL;

  ovr_Shutdown();
}

void* dk2test::getNativeWindowHandle() {
  SDL_SysWMinfo window_info;
  SDL_VERSION(&window_info.version);
  SDL_GetWindowWMInfo(mWindow, &window_info);

#ifdef _WIN32
  return (void*) window_info.info.win.window;
#endif
}

void dk2test::createScene() {
  mSceneManager = mRoot->createSceneManager(Ogre::ST_GENERIC);
  mRootNode = mSceneManager->getRootSceneNode();

  mSceneManager->setSkyBox(true, "Examples/EveningSkyBox");
  mSceneManager->setAmbientLight(ColourValue(0.333f, 0.333f, 0.333f));

  // for gross movement by WASD and control sticks
  Ogre::SceneNode* body_node = mRootNode->createChildSceneNode("BodyNode");

  // for finer movement by positional tracking
  Ogre::SceneNode* head_node = body_node->createChildSceneNode("HeadNode");

  Ogre::Camera* left_eye = mSceneManager->createCamera("LeftEye");
  head_node->attachObject(left_eye);
  left_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(left_eye);

  Ogre::Camera* right_eye = mSceneManager->createCamera("RightEye");
  head_node->attachObject(right_eye);
  right_eye->setNearClipDistance(0.1f);
  mEyeCameras.push_back(right_eye);

  mEyeRenderTarget = mEyeRenderTexture->getBuffer()->getRenderTarget();
  int z_order;
  float left, top, width, height;

  z_order = 0;
  left = 0.0f; top = 0.0;
  width = 0.5f; height = 1.0f;
  mEyeRenderTarget->addViewport(left_eye, z_order, left, top, width, height);

  z_order = 1;
  left = 0.5f; top = 0.0;
  width = 0.5f; height = 1.0f;
  mEyeRenderTarget->addViewport(right_eye, z_order, left, top, width, height);

  mEyeRenderTarget->setAutoUpdated(false);

  body_node->setPosition(Vector3(0, 0, 50));
  body_node->lookAt(Vector3(0, 0, 0), Node::TS_WORLD);

  auto node = mRootNode->createChildSceneNode("ogrehead");
  auto ogrehead = mSceneManager->createEntity("ogrehead.mesh");
  node->attachObject(ogrehead);
  node->setPosition(Vector3(0, 0, 0));
  auto scale = 0.25f;
  node->setScale(Vector3(scale, scale, scale));
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);

  auto light = mSceneManager->createLight();
  node->attachObject(light);
  light->setType(Ogre::Light::LT_POINT);
  light->setPosition(Ogre::Vector3(0, 0, 100));

  node = mRootNode->createChildSceneNode("sinbad");
  auto sinbad = mSceneManager->createEntity("Sinbad.mesh");
  mAnimatingEntities.push_back(sinbad);
  auto anim_state = sinbad->getAnimationState("Dance");
  anim_state->setEnabled(true);
  anim_state->setLoop(true);
  node->attachObject(sinbad);
  scale = 4.0f;
  node->setScale(Vector3(scale, scale, scale));
  node->lookAt(Vector3(0, 0, -1), Node::TS_WORLD);
  node->setPosition(Vector3(-20, 0, 0));
}

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

void dk2test::loop() {
  SDL_Event e;
  Timer* timer = new Timer;

  while (true) {
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          return;
      }
    }

    double us = timer->getMicroseconds() / 1e6;
    timer->reset();

    for (auto entity : mAnimatingEntities) {
      entity->getAnimationState("Dance")->addTime((float)us);
    }

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

  for (int eye_index = 0; eye_index < ovrEye_Count; ++eye_index) {
    ovrEyeType eye = mHmd->EyeRenderOrder[eye_index];
    head_pose[eye] = ovrHmd_GetEyePose(mHmd, eye);

    Quatf orientation = Quatf(head_pose[eye].Orientation);
    Matrix4f proj = ovrMatrix4f_Projection(
        mEyeRenderDesc[eye].Fov, 0.01f, 10000.0f, true);


    auto camera = mEyeCameras[eye_index];
    auto pos = camera->getDerivedPosition();
    Vector3f world_eye_pos(pos.x, pos.y, pos.z);
    Matrix4f view = (Matrix4f(orientation.Inverted()) * Matrix4f::Translation(-world_eye_pos));
    view = Matrix4f::Translation(mEyeRenderDesc[eye].ViewAdjust) * view;

    camera->setCustomViewMatrix(true, ToOgreMatrix(view, m));

    ToOgreMatrix(proj, m);
    camera->setCustomProjectionMatrix(true, m);
  }

  mRoot->renderOneFrame();
  mEyeRenderTarget->update();

  ovrGLTexture gl_eye_textures[2];
  ovrTexture eye_textures[2];
  for (int i = 0; i < ovrEye_Count; ++i ) {
    gl_eye_textures[i].OGL.Header.API = ovrRenderAPI_OpenGL;
    gl_eye_textures[i].OGL.Header.TextureSize = mRenderTargetSize;
    gl_eye_textures[i].OGL.TexId = mEyeRenderTargetTextureId;

    auto& rect = gl_eye_textures[i].OGL.Header.RenderViewport;
    rect.Size = mRenderTargetSize;
    rect.Size.w /= 2;
    rect.Pos.y = 0;

    if (i == ovrEye_Left) {
      rect.Pos.x = 0;
    } else if (i == ovrEye_Right) {
      rect.Pos.x = (mRenderTargetSize.w + 1) / 2;
    }

    eye_textures[i] = gl_eye_textures[i].Texture;
  }

  ovrHmd_EndFrame(mHmd, head_pose, eye_textures);
}
