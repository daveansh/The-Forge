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

//~ Aqueous is a real-time photo realistic ocean rendering library.
//~ This file is a unit test that uses Aqueous to create an ocean app.

//Interfaces
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

const char* pszBases[FSR_Count] = {
	"../../../src/01_Aqueous/",             // FSR_BinShaders
	"../../../src/01_Aqueous/",             // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/01_Aqueous/",             // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

const uint32_t gImageCount = 3;

Renderer* pRenderer = NULL;
Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL; // Cmd buffers.
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
GpuProfiler* pGpuProfiler = NULL;

class Ocean: public IApp
{
	public:
    bool Init();
    void Exit();
    bool Load();
    void Unload();
    void Update(float deltaTime);
    void Draw();
    const char* GetName() override { return "01_Aqueous"; }
};

bool Ocean::Init() 
{
  RendererDesc settings = { 0 };
  initRenderer(GetName(), &settings, &pRenderer);

  // Check for init success.
  if (!pRenderer) 
  {
    return false;
  }
  
  // Create a queue and command pool.
  QueueDesc queueDesc = {};

  // For Vulkan, CMD_POOL_DIRECT is GRAPHICS_QUEUE_BIT
  queueDesc.mType = CMD_POOL_DIRECT;
  queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;

  addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
  addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
  // Add command buffers.
  addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

  // Create syncronization resources.
  for (uint32_t i = 0; i < gImageCount; ++i) 
  {
    addFence(pRenderer, &pRenderCompleteFences[i]);
    addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
  }
  addSemaphore(pRenderer, &pImageAcquiredSemaphore);

  initResourceLoaderInterface(pRenderer);

  // Initialize profile
  initProfiler(pRenderer, gImageCount);
  profileRegisterInput();
  addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");



}


DEFINE_APPLICATION_MAIN(Ocean)
