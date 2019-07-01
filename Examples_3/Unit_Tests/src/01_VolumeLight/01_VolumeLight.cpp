/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

/*
 * Unit Test for a screen space volume lighting solution.
 * https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html
*/

// Interface
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

// Const Data.
const uint32_t gBackBufferImageCount = 3;
const char* pSkyBoxImageFileNames[] = { "Skybox_right1.png",  "Skybox_left2.png",  "Skybox_top3.png",
                    "Skybox_bottom4.png", "Skybox_front5.png", "Skybox_back6.png" };
struct UniformBlock
{
  mat4 mProjectView;
};

// Utils
UIApp gAppUI;
FileSystem gFileSystem;
TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
GpuProfiler* pGpuProfiler = NULL;
GuiComponent* pProfilerGui = NULL;
bool bToggleMicroProfiler = false;
bool bPrevToggleMicroProfiler = false;

// Graphics.
Renderer* pRenderer = NULL;
Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd** ppCmdBuffers = NULL;
Fence* pRenderCompleteFences[gBackBufferImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gBackBufferImageCount] = { NULL };

ICameraController* pCameraController = NULL;
Sampler* pSamplerSkyBox = NULL;
Texture* pSkyBoxTextures[6];
Shader*  pSkyBoxDrawShader = NULL;
RootSignature* pRootSignature = NULL;
DescriptorBinder* pDescriptorBinder = NULL;
RasterizerState* pSkyboxRast = NULL;
DepthState* pDepth = NULL;
Buffer* pSkyBoxVertexBuffer = NULL;
Buffer* pSkyboxUniformBuffer[gBackBufferImageCount] = { NULL };
SwapChain* pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Pipeline* pSkyBoxDrawPipeline = NULL;
UniformBlock gUniformDataSky;
uint32_t gFrameIndex = 0;

const char* pszBases[FSR_Count] = {
  "../../../src/01_Transformations/",     // FSR_BinShaders
  "../../../src/01_Transformations/",     // FSR_SrcShaders
  "../../../UnitTestResources/",          // FSR_Textures
  "../../../UnitTestResources/",          // FSR_Meshes
  "../../../UnitTestResources/",          // FSR_Builtin_Fonts
  "../../../src/01_Transformations/",     // FSR_GpuConfig
  "",                                     // FSR_Animation
  "",                                     // FSR_OtherFiles
  "../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
  "../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

class VolumeLight : public IApp
{
public:
  bool Init();
  void Exit();
  bool Load();
  void Unload();
  void Update(float deltaTime);
  void Draw();
  bool CreateSwapChain();
  bool CreateDepthBuffer();
  void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0));
  const char* GetName() { return "01_VolumeLighting"; }
  static bool CameraInputEvent(const ButtonData* data);
};

bool VolumeLight::Init()
{
  // Renderer Setup
  RendererDesc settings = { 0 };
  initRenderer(GetName(), &settings, &pRenderer);

  // Check for init success
  if (!pRenderer)
    return false;

  // Create a queue and command pool.
  QueueDesc queueDesc = {};
  queueDesc.mType = CMD_POOL_DIRECT;
  queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
  addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
  addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);

  // Create (n) commnadbuffers for use. 
  addCmd_n(pCmdPool, false, gBackBufferImageCount, &ppCmdBuffers);

  // Create sync primitives.
  addSemaphore(pRenderer, &pImageAquiredSemaphore);
  for (uint32_t i = 0; i < gBackBufferImageCount; ++i)
  {
    addFence(pRenderer, &pRenderCompleteFences[i]);
    addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
  }


  initResourceLoaderInterface(pRenderer);

  // Initialize profiler
  initProfiler(pRenderer, gBackBufferImageCount);
  profileRegisterInput();
  addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

  // Load skybox textures.
  for (int i = 0; i < 6; ++i)
  {
    TextureLoadDesc textureDesc = {};
    textureDesc.mRoot = FSR_Textures;
    textureDesc.mUseMipmaps = true;
    textureDesc.pFilename = pSkyBoxImageFileNames[i];
    textureDesc.ppTexture = &pSkyBoxTextures[i];
    addResource(&textureDesc, true);
  }

  // Create shaders.
  ShaderLoadDesc skyShader = {};
  skyShader.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
  skyShader.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };

  addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);

  // Add texture samplers
  SamplerDesc samplerDesc = {};
  samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
  samplerDesc.mMinFilter = FILTER_LINEAR;
  samplerDesc.mMagFilter = FILTER_LINEAR;
  addSampler(pRenderer, &samplerDesc, &pSamplerSkyBox);

  const char* pStaticSamplers[] = { "uSampler0" };
  RootSignatureDesc rootDesc = {};
  rootDesc.mStaticSamplerCount = 1;
  rootDesc.ppStaticSamplers = &pSamplerSkyBox;
  rootDesc.mShaderCount = 1;
  rootDesc.ppShaders = &pSkyBoxDrawShader;
  rootDesc.ppStaticSamplerNames = pStaticSamplers;

  addRootSignature(pRenderer, &rootDesc, &pRootSignature);

  // Todo: What is this?
  DescriptorBinderDesc descriptorBinderDesc[1] = { { pRootSignature } };
  addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDesc, &pDescriptorBinder);

  // Skybox rasterizer state.
  RasterizerStateDesc rasterizerStateDesc = {};
  rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
  addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);

  // skybox depth state.
  DepthStateDesc depthStateDesc = {};
  depthStateDesc.mDepthTest = true;
  depthStateDesc.mDepthWrite = true;
  depthStateDesc.mDepthFunc = CMP_LEQUAL;
  addDepthState(pRenderer, &depthStateDesc, &pDepth);

  // Generate sky box vertex buffer
  float skyBoxPoints[] = {
    10.0f,  -10.0f, -10.0f, 6.0f,    // -z
    -10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
    -10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

    -10.0f, -10.0f, 10.0f,  2.0f,    //-x
    -10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
    -10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

    10.0f,  -10.0f, -10.0f, 1.0f,    //+x
    10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
    10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

    -10.0f, -10.0f, 10.0f,  5.0f,    // +z
    -10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
    10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

    -10.0f, 10.0f,  -10.0f, 3.0f,    //+y
    10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
    10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

    10.0f,  -10.0f, 10.0f,  4.0f,    //-y
    10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
    -10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
  };

  // Add the vertex vyffer res
  uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
  BufferLoadDesc skyboxVbDesc = {};
  skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
  skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
  skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
  skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
  skyboxVbDesc.pData = skyBoxPoints;
  skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
  addResource(&skyboxVbDesc);

  BufferLoadDesc ubDesc = {};
  ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
  ubDesc.mDesc.mSize = sizeof(UniformBlock);
  // Keep the buffer mapped, data will update every frame.
  ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
  ubDesc.pData = NULL;
  
  // Create multiple ubs' for multi-frame rendering.
  for (uint32_t i = 0; i < gBackBufferImageCount; ++i)
  {
    ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
    addResource(&ubDesc);
  }

  finishResourceLoading();
 
  // Init GUI
  if (!gAppUI.Init(pRenderer))
    return false;

  gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

  // Create Micro-Profiler GUI
  GuiDesc guiDesc = {};
  float   dpiScale = getDpiScale().x;
  guiDesc.mStartSize = vec2(140.0f / dpiScale, 320.0f / dpiScale);
  guiDesc.mStartPosition = vec2(mSettings.mWidth - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);
  pProfilerGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);
  pProfilerGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

  // Create FpsCameraController with following properties.
  CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
  vec3                   camPos{ 48.0f, 48.0f, 20.0f };
  vec3                   lookAt{ 0 };

  pCameraController = createFpsCameraController(camPos, lookAt);
  requestMouseCapture(true);
  pCameraController->setMotionParameters(cmp);

  // Register camera controller to get user input data.
  InputSystem::RegisterInputEvent(CameraInputEvent);

  return true;
}

void VolumeLight::Exit()
{
}

bool VolumeLight::Load()
{
  if (!CreateSwapChain())
    return false;

  if (!CreateDepthBuffer())
    return false;

  if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
    return false;

  // Vertex Buffer Layout for Skybox Shader
  VertexLayout vertexLayout = {};
  vertexLayout.mAttribCount = 1;
  vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
  vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
  vertexLayout.mAttribs[0].mBinding = 0;
  vertexLayout.mAttribs[0].mLocation = 0;
  vertexLayout.mAttribs[0].mOffset = 0;

  PipelineDesc desc = {};
  desc.mType = PIPELINE_TYPE_GRAPHICS;
  GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
  pipelineSettings.mRenderTargetCount = 1;
  pipelineSettings.pDepthState = NULL;
  pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
  pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
  pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
  pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
  pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
  pipelineSettings.pRootSignature = pRootSignature;
  pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
  pipelineSettings.pRasterizerState = pSkyboxRast;
  pipelineSettings.pVertexLayout = &vertexLayout;
  addPipeline(pRenderer, &desc, &pSkyBoxDrawPipeline);
  return true;
}

void VolumeLight::Unload()
{
}

void VolumeLight::Update(float deltaTime)
{
  // Update Input and Camera
  if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
  {
    RecenterCameraView(170.0f);
  }

  pCameraController->update(deltaTime);

  static float currentTime = 0.0f;
  currentTime += deltaTime * 1000.0f; //??

  // Update camera with Time.
  mat4 viewMat = pCameraController->getViewMatrix();
  
  const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
  const float horizontal_fov = PI / 2.0f;

  mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

  // Don't walk out of the sky.
  viewMat.setTranslation(vec3(0));

  gUniformDataSky.mProjectView = projMat * viewMat;

  // Profiler.
  if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
  {
    Profile& S = *ProfileGet();
    int nValue = bToggleMicroProfiler ? 1 : 0;
    nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
    S.nDisplay = nValue;

    bPrevToggleMicroProfiler = bToggleMicroProfiler;
  }

  gAppUI.Update(deltaTime);
}

void VolumeLight::Draw()
{
  // Ask the presentation engine for the next image.
  acquireNextImage(pRenderer, pSwapChain, pImageAquiredSemaphore, NULL, &gFrameIndex);

  Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

  // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
  FenceStatus fenceStatus;
  getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
  if (fenceStatus == FENCE_STATUS_INCOMPLETE)
    waitForFences(pRenderer, 1, &pRenderCompleteFence);

  RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
  Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];

  // Update uniform buffers. Todo: Updating uniform buffer resource is not a command?
  BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex], &gUniformDataSky };
  updateResource(&skyboxViewProjCbv);

  // Simply record the screen cleaning command
  LoadActionsDesc loadActions = {};
  loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
  loadActions.mClearColorValues[0].r = 1.0f;
  loadActions.mClearColorValues[0].g = 1.0f;
  loadActions.mClearColorValues[0].b = 0.0f;
  loadActions.mClearColorValues[0].a = 0.0f;
  loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
  loadActions.mClearDepth.depth = 1.0f;
  loadActions.mClearDepth.stencil = 0;

  Cmd* cmd = ppCmdBuffers[gFrameIndex];
  beginCmd(cmd);

  cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

  // Memory barrier for rendertarget and depth target.
  TextureBarrier barriers[] = {
  { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
  { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
  };

  // Only texture barriers.
  cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

  // Bind rendertarget.
  cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
  cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
  cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

  // Draw Skybox.
  cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw skybox", true);
  cmdBindPipeline(cmd, pSkyBoxDrawPipeline);

  DescriptorData params[7] = {};
  params[0].pName = "uniformBlock";
  params[0].ppBuffers = &pSkyboxUniformBuffer[gFrameIndex];
  params[1].pName = "RightText";
  params[1].ppTextures = &pSkyBoxTextures[0];
  params[2].pName = "LeftText";
  params[2].ppTextures = &pSkyBoxTextures[1];
  params[3].pName = "TopText";
  params[3].ppTextures = &pSkyBoxTextures[2];
  params[4].pName = "BotText";
  params[4].ppTextures = &pSkyBoxTextures[3];
  params[5].pName = "FrontText";
  params[5].ppTextures = &pSkyBoxTextures[4];
  params[6].pName = "BackText";
  params[6].ppTextures = &pSkyBoxTextures[5];
  cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 7, params);
  cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, NULL);
  cmdDraw(cmd, 36, 0);
  cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

  cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
  static HiresTimer gTimer;
  gTimer.GetUSec(true);

  gAppUI.DrawText(cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

  gAppUI.Gui(pProfilerGui);
  cmdDrawProfiler(cmd, static_cast<uint32_t>(mSettings.mWidth), static_cast<uint32_t>(mSettings.mHeight));

  gAppUI.Draw(cmd);
  cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
  cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

  barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
  cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

  cmdEndGpuFrameProfile(cmd, pGpuProfiler);
  endCmd(cmd);

  queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAquiredSemaphore, 1, &pRenderCompleteSemaphore);
  queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
  flipProfiler();
}

bool VolumeLight::CreateSwapChain()
{
  SwapChainDesc swapChainDesc = {};
  swapChainDesc.pWindow = pWindow;
  swapChainDesc.mPresentQueueCount = 1;
  swapChainDesc.ppPresentQueues = &pGraphicsQueue;
  swapChainDesc.mWidth = mSettings.mWidth;
  swapChainDesc.mHeight = mSettings.mHeight;
  swapChainDesc.mImageCount = gBackBufferImageCount;
  swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
  swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
  swapChainDesc.mEnableVsync = false;
  ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

  return pSwapChain != NULL;
}

bool VolumeLight::CreateDepthBuffer()
{
  RenderTargetDesc depthRT = {};
  depthRT.mArraySize = 1;
  depthRT.mClearValue.depth = 1.0f;
  depthRT.mClearValue.stencil = 0;
  depthRT.mDepth = 1;
  depthRT.mFormat = ImageFormat::D32F;
  depthRT.mHeight = mSettings.mHeight;
  depthRT.mSampleCount = SAMPLE_COUNT_1;
  depthRT.mSampleQuality = 0;
  depthRT.mWidth = mSettings.mWidth;
  addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
  return pDepthBuffer != NULL;
}

void VolumeLight::RecenterCameraView(float maxDistance, vec3 lookAt)
{
  vec3 p = pCameraController->getViewPosition();
  vec3 d = p - lookAt;

  float lenSqr = lengthSqr(d);
  if (lenSqr > (maxDistance * maxDistance))
  {
    d *= (maxDistance / sqrtf(lenSqr));
  }

  p = d + lookAt;
  pCameraController->moveTo(p);
  pCameraController->lookAt(lookAt);
}

bool VolumeLight::CameraInputEvent(const ButtonData* data)
{
  pCameraController->onInputEvent(data);
  return true;
}

DEFINE_APPLICATION_MAIN(VolumeLight)