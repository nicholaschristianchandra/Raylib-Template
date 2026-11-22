#pragma once

#include <Framework/Framework.hpp>
#include <raymath.h>
#include <rlgl.h>

namespace Program
{
	constexpr unsigned int WINDOW_TITLE_MAX_LENGTH = 128;
	constexpr unsigned int MAX_DUCKS = 16384;
	constexpr int SHADOW_MAP_SIZE = 2048;
	constexpr float SHADOW_AREA = 16.0f;

	enum Animation
	{
		ANIMATION_STATIC,     /* the ducks spin in the vertex shader */
		ANIMATION_GPU_VERTEX, /* the vertex shader moves them every frame */
		ANIMATION_CPU_UPLOAD, /* the CPU moves them and uploads every frame */
		ANIMATION_COUNT
	};

	/* Where a shader's uniforms are, found once when it loads */
	struct LitLocations
	{
		int ViewProjection = -1;
		int Params = -1;
		int DuckA = -1;
		int DuckB = -1;
	};

	struct GpuState
	{
		/* The switches: each one stresses another part of the GPU, or of the CPU feeding it */
		bool VSync = true;
		bool InstancedDraws = true;
		bool Shadows = true;
		bool Bloom = true;
		Animation AnimationMode = ANIMATION_GPU_VERTEX;
		unsigned int ParticleCount = 4096;
		unsigned int DrawCalls = 0;

		/* Render targets: the HDR scene with its depth, two half-size bloom buffers, the shadow map */
		RenderTexture2D Scene = {};
		RenderTexture2D BloomA = {};
		RenderTexture2D BloomB = {};
		RenderTexture2D ShadowMap = {};
		bool Hdr = true;

		Shader Sky = {};
		Shader Lit = {};
		Shader Shadow = {};
		Shader Marker = {};
		Shader Particle = {};
		Shader Bright = {};
		Shader Blur = {};
		Shader Composite = {};
		LitLocations LitAt;
		LitLocations ShadowAt;
		bool ShadersFailed = false;

		Model Duck = {};
		Mesh Sphere = {};
		Mesh Plane = {};
		Texture2D White = {};
		Texture2D FlatNormal = {};
		Texture2D BrickAlbedo = {};
		Texture2D BrickNormal = {};
		Material DuckMaterial = {};
		Material PlaneMaterial = {};
		Material MarkerMaterial = {};
		Material ShadowMaterial = {};
		unsigned int EmptyVertexArray = 0;

		/* Per duck, a Matrix whose first two columns hold x, y, z, scale, then angle, spin, phase: where it starts, and
		   this frame's place when the CPU animates them */
		Matrix *Starts = null;
		Matrix *Animated = null;
		unsigned int PlacedDucks = 0;
	};

	struct State
	{
		String(WINDOW_TITLE_MAX_LENGTH) WindowTitle = { PROGRAM_NAME };
		Vector2 Resolution = {.x = 1280, .y = 720};

		GpuState Gpu;
		Music Song = {};
		bool MusicLoaded = false;
		bool Muted = false;

		unsigned int DuckCount = 1;
		double Time = 0;
		double StatsTime = 0;
		unsigned int StatsFrames = 0;
	};
}
