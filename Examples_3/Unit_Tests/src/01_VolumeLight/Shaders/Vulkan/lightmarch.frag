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

#version 450 core

layout (location = 0) in vec4 UV;
layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform texture2D OcclusionTexture;
layout (set = 0, binding = 1) uniform sampler   uSampler1;

//#define NUM_STEPS = 64;
//#define NUM_DELTA = 0.015625;

void main()
{
  const float maxDeltaLen = 0.006;
  const float distDecay = 0.008;
  const float initDecay = 0.2;
  const int numSteps = 64;
  const float numDelta = 1.0/64.0;

  vec2 dirToLight = (vec2(0.4,0.8) - UV.xy);
  float lenToLight = length(dirToLight);
  dirToLight = normalize(dirToLight);
  float deltaLen = min(maxDeltaLen, lenToLight * numDelta);
  vec2 rayDelta = dirToLight * deltaLen;

  vec2 rayOffset = vec2(0.0 , 0.0);
  float decay = initDecay;
  float rayIntensity = 0.0;

  for(int i = 0; i < numSteps; ++i)
  {
    vec2 sampleLocation = UV.xy + rayOffset;
    float occluded = texture(sampler2D(OcclusionTexture, uSampler1), sampleLocation).r;

    rayIntensity += (1.0 - occluded) * decay;

    rayOffset += rayDelta;

    decay = clamp(decay - distDecay, 0.0, 1.0);
  }

  float col = rayIntensity * 0.15;
	FragColor = vec4(col,col,col, 1.0);
}
