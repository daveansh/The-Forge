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

#pragma once
//--------------------------------------------------------------------------------------------
//
// Copyright (C) 2009 - 2016 Confetti Special Effects Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Special Effects Inc.
//
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// NOTE: Make sure this is the last include in a .cpp file!
//--------------------------------------------------------------------------------------------

#include <new>

void* conf_malloc(size_t size);
void* conf_calloc(size_t count, size_t size);
void* conf_memalign(size_t align, size_t size);
void* conf_realloc(void* ptr, size_t size);
void  conf_free(void* ptr);

template <typename T, typename... Args>
static T* conf_placement_new(void* ptr, Args... args)
{
	return new (ptr) T(args...);
}

template <typename T, typename... Args>
static T* conf_new(Args... args)
{
	T* ptr = (T*)conf_malloc(sizeof(T));
	return new (ptr) T(args...);
}

template <typename T>
static void conf_delete(T* ptr)
{
	ptr->~T();
	conf_free(ptr);
}

#define malloc(size) static_assert(false, "Please use conf_malloc");
#define calloc(count, size) static_assert(false, "Please use conf_calloc");
#define memalign(align, size) static_assert(false, "Please use conf_memalign");
#define realloc(ptr, size) static_assert(false, "Please use conf_realloc");
#define free(ptr) static_assert(false, "Please use conf_free");
#define new static_assert(false, "Please use conf_placement_new");
#define delete static_assert(false, "Please use conf_free with explicit destructor call");
