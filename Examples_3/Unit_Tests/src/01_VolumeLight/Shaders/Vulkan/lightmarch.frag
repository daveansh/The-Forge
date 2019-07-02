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

#define MAX_CUBES 9

layout (location = 0) in vec4 UV;
layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform texture2D OcclusionTexture;

layout (std140, set = 0, binding = 1) uniform uniformBlock {
	  uniform mat4 mvp;
    uniform mat4 mProjectViewSky;
    uniform mat4 toWorld[MAX_CUBES];
    uniform vec4 color[MAX_CUBES];

    // Directional Light Information
    uniform vec3 lightDir;

    // Raymarch info
    uniform vec2 mSSLight;
    uniform float mExposure;
    uniform float mDecay;
};

layout (set = 0, binding = 8) uniform sampler uSampler0;


void main()
{
  const float maxDeltaLen = 0.005;
  const float distDecay = 0.002;
  const int numSteps = 128;
  const float numDelta = 1.0/128.0;

  vec2 dirToLight = (clamp(mSSLight,0.0,1.0) - UV.xy);
  float lenToLight = length(dirToLight);
  dirToLight = normalize(dirToLight);
  float deltaLen = min(maxDeltaLen, lenToLight * numDelta);
  vec2 rayDelta = dirToLight * deltaLen;

  vec2 rayOffset = vec2(0.0 , 0.0);
  float decay = mDecay;
  float rayIntensity = 0.0;

  for(int i = 0; i < numSteps; ++i)
  {
    vec2 sampleLocation = UV.xy + rayOffset;
    float occluded = texture(sampler2D(OcclusionTexture, uSampler0), sampleLocation).r;

    rayIntensity += (1.0 - occluded) * decay;
    rayOffset += rayDelta;
    decay = clamp(decay - distDecay, 0.0, 1.0);
  }

  vec3 color = vec3(1.0, 1.0, 1.0) * rayIntensity * mExposure;
	FragColor = vec4(color, 1.0);
}
