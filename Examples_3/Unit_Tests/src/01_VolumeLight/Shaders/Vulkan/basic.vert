#version 450 core

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


// Shader for simple shading with a point light
// for planets in Unit Test 12 - Transformations

#define MAX_CUBES 9

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;

layout(location = 0) out vec4 Color;

layout (std140, set=0, binding=1) uniform uniformBlock {
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

void main ()
{
	mat4 tempMat = mvp * toWorld[gl_InstanceIndex];
	gl_Position = tempMat * vec4(Position.xyz, 1.0f);
	
	vec4 normal = normalize(toWorld[gl_InstanceIndex] * vec4(Normal.xyz, 0.0f));
	vec4 pos = toWorld[gl_InstanceIndex] * vec4(Position.xyz, 1.0f);
	
	float lightIntensity = 1.0f;
  float ambientCoeff = 0.4;
	
  vec3 baseColor = color[gl_InstanceIndex].xyz;
  vec3 blendedColor = baseColor * lightIntensity;
  vec3 diffuse = blendedColor * max(dot(normal.xyz, lightDir), 0.0);
  vec3 ambient = baseColor * ambientCoeff;
  Color = vec4(ambient + diffuse, 1.0);
}
