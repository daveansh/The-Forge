# SPIRV-Cross

SPIRV-Cross is a tool designed for parsing and converting SPIR-V to other shader languages.

[![Build Status](https://travis-ci.org/KhronosGroup/SPIRV-Cross.svg?branch=master)](https://travis-ci.org/KhronosGroup/SPIRV-Cross)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/KhronosGroup/SPIRV-Cross?svg=true&branch=master)](https://ci.appveyor.com/project/HansKristian-ARM/SPIRV-Cross)

## Features

  - Convert SPIR-V to readable, usable and efficient GLSL
  - Convert SPIR-V to readable, usable and efficient Metal Shading Language (MSL)
  - Convert SPIR-V to readable, usable and efficient HLSL
  - Convert SPIR-V to debuggable C++ [EXPERIMENTAL]
  - Convert SPIR-V to a JSON reflection format [EXPERIMENTAL]
  - Reflection API to simplify the creation of Vulkan pipeline layouts
  - Reflection API to modify and tweak OpDecorations
  - Supports "all" of vertex, fragment, tessellation, geometry and compute shaders.

SPIRV-Cross tries hard to emit readable and clean output from the SPIR-V.
The goal is to emit GLSL or MSL that looks like it was written by a human and not awkward IR/assembly-like code.

NOTE: Individual features are expected to be mostly complete, but it is possible that certain obscure GLSL features are not yet supported.
However, most missing features are expected to be "trivial" improvements at this stage.

## Building

SPIRV-Cross has been tested on Linux, OSX and Windows.

The make and CMake build flavors offer the option to treat exceptions as assertions. To disable exceptions for make just append `SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=1` to the command line. For CMake append `-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=ON`. By default exceptions are enabled.

### Linux and macOS

Just run `make` on the command line. A recent GCC (4.8+) or Clang (3.x+) compiler is required as SPIRV-Cross uses C++11 extensively.

### Windows

MinGW-w64 based compilation works with `make`, and an MSVC 2013 solution is also included.

## Usage

### Using the C++ API

For more in-depth documentation than what's provided in this README, please have a look at the [Wiki](https://github.com/KhronosGroup/SPIRV-Cross/wiki).

To perform reflection and convert to other shader languages you can use the SPIRV-Cross API.
For example:

```
#include "spirv_glsl.hpp"
#include <vector>
#include <utility>

extern std::vector<uint32_t> load_spirv_file();

int main()
{
	// Read SPIR-V from disk or similar.
	std::vector<uint32_t> spirv_binary = load_spirv_file();

	spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

	// The SPIR-V is now parsed, and we can perform reflection on it.
	spirv_cross::ShaderResources resources = glsl.get_shader_resources();

	// Get all sampled images in the shader.
	for (auto &resource : resources.sampled_images)
	{
		unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

		// Modify the decoration to prepare it for GLSL.
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

		// Some arbitrary remapping if we want.
		glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
	}

	// Set some options.
	spirv_cross::CompilerGLSL::Options options;
	options.version = 310;
	options.es = true;
	glsl.set_options(options);

	// Compile to GLSL, ready to give to GL driver.
	std::string source = glsl.compile();
}
```

#### Integrating SPIRV-Cross in a custom build system

To add SPIRV-Cross to your own codebase, just copy the source and header files from root directory
and build the relevant .cpp files you need. Make sure to build with C++11 support, e.g. `-std=c++14` in GCC and Clang.
Alternatively, the Makefile generates a libspirv-cross.a static library during build that can be linked in.

### Creating a SPIR-V file from GLSL with glslang

```
glslangValidator -H -V -o test.spv test.frag
```

### Converting a SPIR-V file to GLSL ES

```
glslangValidator -H -V -o test.spv shaders/comp/basic.comp
./spirv-cross --version 310 --es test.spv
```

#### Converting to desktop GLSL

```
glslangValidator -H -V -o test.spv shaders/comp/basic.comp
./spirv-cross --version 330 test.spv --output test.comp
```

#### Disable prettifying optimizations

```
glslangValidator -H -V -o test.spv shaders/comp/basic.comp
./spirv-cross --version 310 --es test.spv --output test.comp --force-temporary
```

### Using shaders generated from C++ backend

Please see `samples/cpp` where some GLSL shaders are compiled to SPIR-V, decompiled to C++ and run with test data.
Reading through the samples should explain how to use the C++ interface.
A simple Makefile is included to build all shaders in the directory.

### Implementation notes

When using SPIR-V and SPIRV-Cross as an intermediate step for cross-compiling between high level languages there are some considerations to take into account,
as not all features used by one high-level language are necessarily supported natively by the target shader language.
SPIRV-Cross aims to provide the tools needed to handle these scenarios in a clean and robust way, but some manual action is required to maintain compatibility.

#### HLSL source to GLSL

##### HLSL entry points

When using SPIR-V shaders compiled from HLSL, there are some extra things you need to take care of.
First make sure that the entry point is used correctly.
If you forget to set the entry point correctly in glslangValidator (-e MyFancyEntryPoint),
you will likely encounter this error message:

```
Cannot end a function before ending the current block.
Likely cause: If this SPIR-V was created from glslang HLSL, make sure the entry point is valid.
```

##### Vertex/Fragment interface linking

HLSL relies on semantics in order to effectively link together shader stages. In the SPIR-V generated by glslang, the transformation from HLSL to GLSL ends up looking like

```
struct VSOutput {
   // SV_Position is rerouted to gl_Position
   float4 position : SV_Position;
   float4 coord : TEXCOORD0;
};

VSOutput main(...) {}
```

```
struct VSOutput {
   float4 coord;
}
layout(location = 0) out VSOutput _magicNameGeneratedByGlslang;
```

While this works, be aware of the type of the struct which is used in the vertex stage and the fragment stage.
There may be issues if the structure type name differs in vertex stage and fragment stage.

You can make use of the reflection interface to force the name of the struct type.

```
// Something like this for both vertex outputs and fragment inputs.
compiler.set_name(varying_resource.base_type_id, "VertexFragmentLinkage");
```

Some platform may require identical variable name for both vertex outputs and fragment inputs. (for example MacOSX)
to rename varaible base on location, please add
```
--rename-interface-variable <in|out> <location> <new_variable_name>
```

#### HLSL source to legacy GLSL/ESSL

HLSL tends to emit varying struct types to pass data between vertex and fragment.
This is not supported in legacy GL/GLES targets, so to support this, varying structs are flattened.
This is done automatically, but the API user might need to be aware that this is happening in order to support all cases.

Modern GLES code like this:
```
struct Output {
   vec4 a;
   vec2 b;
};
out Output vout;
```

Is transformed into:
```
struct Output {
   vec4 a;
   vec2 b;
};
varying vec4 Output_a;
varying vec2 Output_b;
```

Note that now, both the struct name and the member names will participate in the linking interface between vertex and fragment, so
API users might want to ensure that both the struct names and member names match so that vertex outputs and fragment inputs can link properly.


#### Separate image samplers (HLSL/Vulkan) for backends which do not support it (GLSL)

Another thing you need to remember is when using samplers and textures in HLSL these are separable, and not directly compatible with GLSL. If you need to use this with desktop GL/GLES, you need to call `Compiler::build_combined_image_samplers` first before calling `Compiler::compile`, or you will get an exception.

```
// From main.cpp
// Builds a mapping for all combinations of images and samplers.
compiler->build_combined_image_samplers();

// Give the remapped combined samplers new names.
// Here you can also set up decorations if you want (binding = #N).
for (auto &remap : compiler->get_combined_image_samplers())
{
   compiler->set_name(remap.combined_id, join("SPIRV_Cross_Combined", compiler->get_name(remap.image_id),
            compiler->get_name(remap.sampler_id)));
}
```

If your target is Vulkan GLSL, `--vulkan-semantics` will emit separate image samplers as you'd expect.
The command line client calls `Compiler::build_combined_image_samplers` automatically, but if you're calling the library, you'll need to do this yourself.

#### Descriptor sets (Vulkan GLSL) for backends which do not support them (HLSL/GLSL/Metal)

Descriptor sets are unique to Vulkan, so make sure that descriptor set + binding is remapped to a flat binding scheme (set always 0), so that other APIs can make sense of the bindings.
This can be done with `Compiler::set_decoration(id, spv::DecorationDescriptorSet)`.

#### Linking by name for targets which do not support explicit locations (legacy GLSL/ESSL)

Modern GLSL and HLSL sources (and SPIR-V) relies on explicit layout(location) qualifiers to guide the linking process between shader stages,
but older GLSL relies on symbol names to perform the linking. When emitting shaders with older versions, these layout statements will be removed,
so it is important that the API user ensures that the names of I/O variables are sanitized so that linking will work properly.
The reflection API can rename variables, struct types and struct members to deal with these scenarios using `Compiler::set_name` and friends.

#### Clip-space conventions

SPIRV-Cross can perform some common clip space conversions on gl_Position/SV_Position by enabling `CompilerGLSL::Options.vertex.fixup_clipspace`.
While this can be convenient, it is recommended to modify the projection matrices instead as that can achieve the same result.

For GLSL targets, enabling this will convert a shader which assumes `[0, w]` depth range (Vulkan / D3D / Metal) into `[-w, w]` range.
For MSL and HLSL targets, enabling this will convert a shader in `[-w, w]` depth range (OpenGL) to `[0, w]` depth range.

By default, the CLI will not enable `fixup_clipspace`, but in the API you might want to set an explicit value using `CompilerGLSL::set_options()`.

Y-flipping of gl_Position and similar is also supported.
The use of this is discouraged, because relying on vertex shader Y-flipping tends to get quite messy.
To enable this, set `CompilerGLSL::Options.vertex.flip_vert_y` or `--flip-vert-y` in CLI.

## Contributing

Contributions to SPIRV-Cross are welcome. See Testing and Licensing sections for details.

### Testing

SPIRV-Cross maintains a test suite of shaders with reference output of how the output looks after going through a roundtrip through
glslangValidator then back through SPIRV-Cross again. The reference files are stored inside the repository in order to be able to track regressions.

All pull requests should ensure that test output does not change unexpectedly. This can be tested with:

```
./test_shaders.py shaders
./test_shaders.py shaders --opt
./test_shaders.py shaders-hlsl --hlsl
./test_shaders.py shaders-hlsl --hlsl --opt
./test_shaders.py shaders-msl --msl
./test_shaders.py shaders-msl --msl --opt
```

although there are a couple of convenience script for doing this:

```
./checkout_glslang_spirv_tools.sh # Checks out glslang and SPIRV-Tools at a fixed revision which matches the reference output.
./build_glslang_spirv_tools.sh    # Builds glslang and SPIRV-Tools.
./test_shaders.sh                 # Runs over all changes and makes sure that there are no deltas compared to reference files.
```

However, when improving SPIRV-Cross there are of course legitimate cases where reference output should change.
In these cases, run:

```
./update_test_shaders.sh
```

to update the reference files and include these changes as part of the pull request.
Always make sure you are running the correct version of glslangValidator as well as SPIRV-Tools when updating reference files.
See `checkout_glslang_spirv_tools.sh`.

In short, the master branch should always be able to run `./test_shaders.py shaders` and friends without failure.
SPIRV-Cross uses Travis CI to test all pull requests, so it is not strictly needed to perform testing yourself if you have problems running it locally.
A pull request which does not pass testing on Travis will not be accepted however.

When adding support for new features to SPIRV-Cross, a new shader and reference file should be added which covers usage of the new shader features in question.

### Licensing

Contributors of new files should add a copyright header at the top of every new source code file with their copyright
along with the Apache 2.0 licensing stub.

### Formatting

SPIRV-Cross uses `clang-format` to automatically format code.
Please use `clang-format` with the style sheet found in `.clang-format` to automatically format code before submitting a pull request.

To make things easy, the `format_all.sh` script can be used to format all
source files in the library. In this directory, run the following from the
command line:

	./format_all.sh

## ABI concerns

### SPIR-V headers

The current repository uses the latest SPIR-V and GLSL.std.450 headers.
SPIR-V files created from older headers could have ABI issues.

## Regression testing

In shaders/ a collection of shaders are maintained for purposes of regression testing.
The current reference output is contained in reference/.
`./test_shaders.py shaders` can be run to perform regression testing.

See `./test_shaders.py --help` for more.

### Metal backend

To test the roundtrip path GLSL -> SPIR-V -> MSL, `--msl` can be added, e.g. `./test_shaders.py --msl shaders-msl`.

### HLSL backend

To test the roundtrip path GLSL -> SPIR-V -> HLSL, `--hlsl` can be added, e.g. `./test_shaders.py --hlsl shaders-hlsl`.

### Updating regression tests

When legitimate changes are found, use `--update` flag to update regression files.
Otherwise, `./test_shaders.py` will fail with error code.

### Mali Offline Compiler cycle counts

To obtain a CSV of static shader cycle counts before and after going through spirv-cross, add
`--malisc` flag to `./test_shaders`. This requires the Mali Offline Compiler to be installed in PATH.

