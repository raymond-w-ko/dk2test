#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

using namespace Ogre;

dk2test::dk2test() {
  this->changeToDirectoryOfExecutable();
  this->initOVR();
  this->initSDL();
  this->initOgre();
  this->initOVR2();

  this->createScene();
  this->createRenderTextureViewer();
}

dk2test::~dk2test() {
  this->destroySDL();
  //this->destroyOVR();
}

void dk2test::changeToDirectoryOfExecutable() {
#if defined(_WIN32)
	TCHAR szFullModulePath[_MAX_PATH + 1];
	GetModuleFileName(NULL, szFullModulePath, _MAX_PATH);
	szFullModulePath[_MAX_PATH] = '\0';

	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFname[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];

	_tsplitpath_s(szFullModulePath,
               szDrive, _MAX_DRIVE,
               szDir, _MAX_DIR,
               szFname, _MAX_FNAME,
               szExt, _MAX_EXT);

  TCHAR szWorkingDir[_MAX_PATH];
  _tmakepath_s( szWorkingDir, _MAX_PATH, szDrive, szDir, NULL, NULL );

  std::wstring working_dir(szWorkingDir);
  working_dir += L"\\..";
	_tchdir(working_dir.c_str());
#else
# error "changeToDirectoryOfExecutable() NYI"
#endif
}

void dk2test::initOVR() {
  ovr_Initialize();

  mHmd = ovrHmd_Create(0);
  if (!mHmd) {
    notice("Could not find a real Oculus head mounted display! Creating a debug one based on the DK1.");
    mHmd = ovrHmd_CreateDebug(ovrHmd_DK1);
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
      //SDL_WINDOW_BORDERLESS |
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
  mRenderWindow->setAutoUpdated(true);
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
  int texture_id;
  mEyeRenderTexture->getCustomAttribute("GLID", &texture_id);

  // might have changed due to hardware limitations
  mRenderTargetSize.w = mEyeRenderTexture->getWidth();
  mRenderTargetSize.h = mEyeRenderTexture->getHeight();

  ovrGLConfig cfg;
  cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
  cfg.OGL.Header.RTSize = mRenderTargetSize;
  cfg.OGL.Header.Multisample = mEyeRenderMultisample;
  cfg.OGL.Window = (decltype(cfg.OGL.Window)) this->getNativeWindowHandle();
  cfg.OGL.DC = NULL;

  int distortionCaps = 0;
  ovrEyeRenderDesc eyeRenderDesc[2];
  
  ovrBool result = ovrHmd_ConfigureRendering(
      mHmd, &cfg.Config, distortionCaps,
      mHmd->DefaultEyeFov, eyeRenderDesc);
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

  mSceneManager->setSkyBox(true, "Examples/SpaceSkyBox");
  mSceneManager->setAmbientLight(ColourValue(0.333f, 0.333f, 0.333f));

  // for gross movement by WASD and control sticks
  Ogre::SceneNode* body_node = mRootNode->createChildSceneNode("BodyNode");

  // for finer movement by positional tracking
  Ogre::SceneNode* head_node = body_node->createChildSceneNode("HeadNode");

  Ogre::Camera* left_eye = mSceneManager->createCamera("LeftEye");
  head_node->attachObject(left_eye);
  left_eye->setNearClipDistance(0.1f);
  Ogre::Camera* right_eye = mSceneManager->createCamera("RightEye");
  head_node->attachObject(right_eye);
  right_eye->setNearClipDistance(0.1f);

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

  body_node->setPosition(Vector3(0, 0, 100));
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
  bool quit = false;
  Timer* timer = new Timer;

  while (!quit) {
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          quit = true;
      }
    }

    double us = timer->getMicroseconds() / 1e6;
    timer->reset();

    for (auto entity : mAnimatingEntities) {
      entity->getAnimationState("Dance")->addTime(us);
    }

    // render here
    mEyeRenderTarget->update();
    mRoot->renderOneFrame();
  }
}
