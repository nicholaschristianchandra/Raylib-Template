#pragma once

#include <Framework/Framework.hpp>

namespace Gpu
{
	/* Every shader, linked again on every hot reload so shader edits show up; a failed one is raylib's default shader */
	bool LoadShaders(Program::GpuState *gpu)
	{
		gpu->Sky = Assets::LoadShader("Shaders/Screen.vert", "Shaders/Sky.frag");
		gpu->Lit = Assets::LoadShader("Shaders/Lit.vert", "Shaders/Lit.frag");
		gpu->Shadow = Assets::LoadShader("Shaders/Lit.vert", "Shaders/Shadow.frag");
		gpu->Marker = Assets::LoadShader("Shaders/Marker.vert", "Shaders/Marker.frag");
		gpu->Particle = Assets::LoadShader("Shaders/Particle.vert", "Shaders/Particle.frag");
		gpu->Bright = Assets::LoadShader("Shaders/Screen.vert", "Shaders/Bright.frag");
		gpu->Blur = Assets::LoadShader("Shaders/Screen.vert", "Shaders/Blur.frag");
		gpu->Composite = Assets::LoadShader("Shaders/Screen.vert", "Shaders/Composite.frag");
		const Shader shaders[8] = {gpu->Sky, gpu->Lit, gpu->Shadow, gpu->Marker, gpu->Particle, gpu->Bright, gpu->Blur, gpu->Composite};
		gpu->ShadersFailed = false;

		for (const Shader &shader : shaders)
		{
			gpu->ShadersFailed |= !IsShaderValid(shader) || shader.id == rlGetShaderIdDefault();
		}

		if (gpu->ShadersFailed)
		{
			return false;
		}

		/* The shadow map is the lit shader's fourth texture, after albedo and the normal map (texture0, texture2) */
		gpu->Lit.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(gpu->Lit, "shadowMap");
		Shader *withDucks[2] = {&gpu->Lit, &gpu->Shadow};
		Program::LitLocations *locations[2] = {&gpu->LitAt, &gpu->ShadowAt};

		for (int i = 0; i < 2; i++)
		{
			locations[i]->ViewProjection = GetShaderLocation(*withDucks[i], "viewProjection");
			locations[i]->Params = GetShaderLocation(*withDucks[i], "params");
			locations[i]->DuckA = GetShaderLocation(*withDucks[i], "duckA");
			locations[i]->DuckB = GetShaderLocation(*withDucks[i], "duckB");
		}

		gpu->DuckMaterial.shader = gpu->Lit;
		gpu->PlaneMaterial.shader = gpu->Lit;
		gpu->MarkerMaterial.shader = gpu->Marker;
		gpu->ShadowMaterial.shader = gpu->Shadow;
		return !gpu->ShadersFailed;
	}

	void UnloadShaders(Program::GpuState *gpu)
	{
		const Shader shaders[8] = {gpu->Sky, gpu->Lit, gpu->Shadow, gpu->Marker, gpu->Particle, gpu->Bright, gpu->Blur, gpu->Composite};

		for (const Shader &shader : shaders)
		{
			UnloadShader(shader);
		}
	}

	/* A render target with a half-float color texture (HDR) when the GPU can draw into one, 8 bits per channel otherwise */
	static RenderTexture2D MakeTarget(int width, int height, bool depth, bool *hdr)
	{
		RenderTexture2D target = {};
		target.id = rlLoadFramebuffer();
		const int format = *hdr ? PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
		target.texture = Texture2D{rlLoadTexture(null, width, height, format, 1), width, height, 1, format};
		rlTextureParameters(target.texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
		rlTextureParameters(target.texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
		rlTextureParameters(target.texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
		rlTextureParameters(target.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
		rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

		if (depth)
		{
			target.depth = Texture2D{rlLoadTextureDepth(width, height, true), width, height, 1, 19};
			rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
		}

		if (!rlFramebufferComplete(target.id) && *hdr)
		{
			TraceLog(LOG_WARNING, "[Gpu] This GPU can't draw into half-float textures: no HDR, so bright light is cut at white");
			UnloadRenderTexture(target);
			*hdr = false;
			return MakeTarget(width, height, depth, hdr);
		}

		return target;
	}

	/* The scene and bloom targets at the window's size, made again when it changes, and the shadow map once */
	static void EnsureTargets(Program::GpuState *gpu, int width, int height)
	{
		if (!gpu->ShadowMap.id)
		{
			gpu->ShadowMap.id = rlLoadFramebuffer();
			gpu->ShadowMap.depth = Texture2D{rlLoadTextureDepth(Program::SHADOW_MAP_SIZE, Program::SHADOW_MAP_SIZE, false), Program::SHADOW_MAP_SIZE, Program::SHADOW_MAP_SIZE, 1, 19};
			gpu->ShadowMap.texture.width = Program::SHADOW_MAP_SIZE;
			gpu->ShadowMap.texture.height = Program::SHADOW_MAP_SIZE;
			rlFramebufferAttach(gpu->ShadowMap.id, gpu->ShadowMap.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

			if (!rlFramebufferComplete(gpu->ShadowMap.id))
			{
				TraceLog(LOG_WARNING, "[Gpu] No shadow map on this GPU");
			}

			gpu->DuckMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = gpu->ShadowMap.depth;
			gpu->PlaneMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = gpu->ShadowMap.depth;
		}

		if (gpu->Scene.texture.width == width && gpu->Scene.texture.height == height)
		{
			return;
		}

		UnloadRenderTexture(gpu->Scene);
		UnloadRenderTexture(gpu->BloomA);
		UnloadRenderTexture(gpu->BloomB);
		gpu->Scene = MakeTarget(width, height, true, &gpu->Hdr);
		gpu->BloomA = MakeTarget(width / 2 > 1 ? width / 2 : 1, height / 2 > 1 ? height / 2 : 1, false, &gpu->Hdr);
		gpu->BloomB = MakeTarget(gpu->BloomA.texture.width, gpu->BloomA.texture.height, false, &gpu->Hdr);
	}

	void Unload(Program::GpuState *gpu)
	{
		UnloadRenderTexture(gpu->Scene);
		UnloadRenderTexture(gpu->BloomA);
		UnloadRenderTexture(gpu->BloomB);
		UnloadRenderTexture(gpu->ShadowMap);
	}

	void ToggleVSync(Program::GpuState *gpu)
	{
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
		gpu->VSync = !gpu->VSync;

		if (gpu->VSync)
		{
			SetWindowState(FLAG_VSYNC_HINT);
		}
		else
		{
			ClearWindowState(FLAG_VSYNC_HINT);
		}
#endif
	}

	static void SetVector(Shader shader, const char *name, Vector3 value, float w)
	{
		const Vector4 vector = {value.x, value.y, value.z, w};
		SetShaderValue(shader, GetShaderLocation(shader, name), &vector, SHADER_UNIFORM_VEC4);
	}

	/* This frame's duck places: the starts, made when the count changes, or the CPU's animation of them */
	static const Matrix *DuckData(Program::State *state, float time)
	{
		Program::GpuState *gpu = &state->Gpu;

		for (; gpu->PlacedDucks < state->DuckCount; gpu->PlacedDucks++)
		{
			Scene::PlaceDuck(gpu->PlacedDucks, &gpu->Starts[gpu->PlacedDucks]);
		}

		if (gpu->AnimationMode != Program::ANIMATION_CPU_UPLOAD)
		{
			return gpu->Starts;
		}

		for (unsigned int i = 0; i < state->DuckCount; i++)
		{
			Scene::AnimateDuck(&gpu->Starts[i], time, &gpu->Animated[i]);
		}

		return gpu->Animated;
	}

	/* Every duck, in one instanced draw (DrawMeshInstanced uploads their data each time), or one draw each */
	static void DrawDucks(Program::State *state, Material material, const Program::LitLocations *at, Matrix viewProjection, const Matrix *ducks, float time)
	{
		Program::GpuState *gpu = &state->Gpu;
		const Mesh duck = gpu->Duck.meshes[0];
		const float animation[Program::ANIMATION_COUNT] = {1, 2, 0};
		Vector4 params = {gpu->InstancedDraws ? 1.0f : 2.0f, time, 0, animation[gpu->AnimationMode]};
		SetShaderValueMatrix(material.shader, at->ViewProjection, viewProjection);
		SetShaderValue(material.shader, at->Params, &params, SHADER_UNIFORM_VEC4);

		if (gpu->InstancedDraws)
		{
			DrawMeshInstanced(duck, material, ducks, (int)state->DuckCount);
			gpu->DrawCalls++;
			return;
		}

		for (unsigned int i = 0; i < state->DuckCount; i++)
		{
			const Vector4 a = {ducks[i].m0, ducks[i].m1, ducks[i].m2, ducks[i].m3};
			const Vector4 b = {ducks[i].m4, ducks[i].m5, ducks[i].m6, ducks[i].m7};
			SetShaderValue(material.shader, at->DuckA, &a, SHADER_UNIFORM_VEC4);
			SetShaderValue(material.shader, at->DuckB, &b, SHADER_UNIFORM_VEC4);
			DrawMesh(duck, material, MatrixIdentity());
		}

		gpu->DrawCalls += state->DuckCount;
	}

	/* One full-screen pass: source drawn into target through a shader */
	static void PostPass(Program::GpuState *gpu, Shader shader, Texture2D source, RenderTexture2D *target, int width, int height)
	{
		if (target)
		{
			BeginTextureMode(*target);
		}

		BeginShaderMode(shader);
		DrawTexturePro(source, Rectangle{0, 0, (float)source.width, -(float)source.height}, Rectangle{0, 0, (float)width, (float)height}, Vector2{0, 0}, 0, WHITE);
		EndShaderMode();

		if (target)
		{
			EndTextureMode();
		}

		gpu->DrawCalls++;
	}

	/* One frame: the shadow map, the HDR scene (sky, bricks, ducks, lights, fireflies), bloom, then the screen */
	void Frame(Program::State *state)
	{
		Program::GpuState *gpu = &state->Gpu;
		const int width = GetRenderWidth();
		const int height = GetRenderHeight();
		EnsureTargets(gpu, width, height);
		gpu->DrawCalls = 0;

		const float time = (float)state->Time;
		const float aspect = (float)width / (float)(height > 0 ? height : 1);
		const float fov = 55.0f * DEG2RAD;
		const Vector3 worldUp = {0, 1, 0};
		const Matrix *ducks = DuckData(state, time);

		/* The camera circles counterclockwise (seen from above), slowly */
		const float cameraAngle = -time * 0.08f;
		const Vector3 target = {0, 0.75f, 0};
		const Vector3 eye = {cosf(cameraAngle) * 5.5f, 2.3f, sinf(cameraAngle) * 5.5f};
		const Matrix viewProjection = MatrixMultiply(MatrixLookAt(eye, target, worldUp), MatrixPerspective(fov, aspect, 0.05f, 400.0f));
		const Vector3 forward = Vector3Normalize(target - eye);
		const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, worldUp));
		const Vector3 up = Vector3CrossProduct(right, forward);

		/* A day every 60 seconds, starting in the morning. The sun casts the shadows by day, the moon by night */
		const float sunAngle = time * 2 * PI / 60.0f + 0.4f;
		const Vector3 sun = Vector3Normalize(Vector3{cosf(sunAngle), sinf(sunAngle), 0.35f});
		const Vector3 moon = sun * -1.0f;
		const float day = Scene::SmoothStep(-0.15f, 0.25f, sun.y);
		const float sunset = Clamp(1.0f - fabsf(sun.y) * 4.0f, 0.0f, 1.0f);
		const Vector3 horizon = Vector3Lerp(Vector3{0.02f, 0.03f, 0.07f}, Vector3{0.62f, 0.75f, 0.90f}, day) + Vector3{0.55f, 0.22f, 0.06f} * sunset;
		const bool moonCasts = sun.y < 0;
		const Vector3 caster = moonCasts ? moon : sun;
		const Vector3 casterUp = fabsf(caster.y) > 0.99f ? Vector3{0, 0, 1} : worldUp;
		const float area = Program::SHADOW_AREA;
		const Matrix lightViewProjection = MatrixMultiply(MatrixLookAt(caster * 40.0f, Vector3{}, casterUp), MatrixOrtho(-area, area, -area, area, 1.0f, 90.0f));

		/* The four lights circle clockwise (seen from above) around the duck */
		const Vector3 lightColors[4] = {{1.0f, 0.12f, 0.1f}, {1.0f, 0.8f, 0.1f}, {0.1f, 1.0f, 0.2f}, {0.15f, 0.35f, 1.0f}};
		Vector4 lightPositions[4];
		Vector4 lightColorValues[4];
		Matrix markers[4];

		for (int i = 0; i < 4; i++)
		{
			const float angle = time * 1.2f + i * PI * 0.5f;
			const Vector3 position = {cosf(angle) * 1.7f, 1.1f + 0.15f * sinf(time * 2.0f + i), sinf(angle) * 1.7f};
			lightPositions[i] = Vector4{position.x, position.y, position.z, 5.0f};
			lightColorValues[i] = Vector4{lightColors[i].x, lightColors[i].y, lightColors[i].z, 3.0f};
			markers[i] = MatrixMultiply(MatrixScale(0.09f, 0.09f, 0.09f), MatrixTranslate(position.x, position.y, position.z));
		}

		/* The shadow map: every duck again, seen from the sun or the moon */
		if (gpu->Shadows)
		{
			BeginTextureMode(gpu->ShadowMap);
			rlEnableDepthTest();
			rlEnableDepthMask();
			rlClearScreenBuffers();
			rlDisableBackfaceCulling();
			DrawDucks(state, gpu->ShadowMaterial, &gpu->ShadowAt, lightViewProjection, ducks, time);
			rlEnableBackfaceCulling();
			rlDisableDepthTest();
			EndTextureMode();
		}

		/* The scene, in HDR: the sky first, behind everything */
		BeginTextureMode(gpu->Scene);
		ClearBackground(BLACK);
		BeginShaderMode(gpu->Sky);
		SetVector(gpu->Sky, "right", right, tanf(fov * 0.5f) * aspect);
		SetVector(gpu->Sky, "up", up, tanf(fov * 0.5f));
		SetVector(gpu->Sky, "forward", forward, 0);
		SetVector(gpu->Sky, "sunDirection", sun, time);
		DrawTexturePro(gpu->White, Rectangle{0, 0, 1, 1}, Rectangle{0, 0, (float)width, (float)height}, Vector2{0, 0}, 0, WHITE);
		EndShaderMode();
		gpu->DrawCalls++;

		rlDrawRenderBatchActive();
		rlEnableDepthTest();
		const Shader lit = gpu->Lit;
		const Vector4 shadowParams = {gpu->Shadows ? 1.0f : 0.0f, 1.0f / Program::SHADOW_MAP_SIZE, moonCasts ? 1.0f : 0.0f, 0};
		SetShaderValueMatrix(lit, GetShaderLocation(lit, "lightViewProjection"), lightViewProjection);
		SetShaderValue(lit, GetShaderLocation(lit, "shadowParams"), &shadowParams, SHADER_UNIFORM_VEC4);
		SetVector(lit, "cameraPosition", eye, 0);
		SetVector(lit, "sunDirection", sun, 0);
		SetVector(lit, "sunColor", (Vector3{1.0f, 0.93f, 0.82f} + Vector3{0.2f, -0.25f, -0.4f} * sunset) * (2.6f * day), 0);
		SetVector(lit, "moonDirection", moon, 0);
		SetVector(lit, "moonColor", Vector3{0.25f, 0.32f, 0.5f} * (0.5f * (1 - day)), 0);
		SetVector(lit, "ambient", Vector3Lerp(Vector3{0.03f, 0.03f, 0.04f}, Vector3{0.22f, 0.27f, 0.35f}, day), 0);
		SetVector(lit, "fog", horizon, 0.012f);
		SetShaderValueV(lit, GetShaderLocation(lit, "lightPositions"), lightPositions, SHADER_UNIFORM_VEC4, 4);
		SetShaderValueV(lit, GetShaderLocation(lit, "lightColors"), lightColorValues, SHADER_UNIFORM_VEC4, 4);

		/* The bricks: normal mapped, their texture placed by world position */
		const Vector4 ground = {0, time, 0, 0};
		SetShaderValueMatrix(lit, gpu->LitAt.ViewProjection, viewProjection);
		SetShaderValue(lit, gpu->LitAt.Params, &ground, SHADER_UNIFORM_VEC4);
		SetVector(lit, "material", Vector3{1, 0.25f, 24}, 1);
		DrawMesh(gpu->Plane, gpu->PlaneMaterial, MatrixIdentity());
		gpu->DrawCalls++;

		SetVector(lit, "material", Vector3{0, 0.6f, 48}, 0);
		DrawDucks(state, gpu->DuckMaterial, &gpu->LitAt, viewProjection, ducks, time);

		/* A small glowing sphere at each light */
		SetShaderValueMatrix(gpu->Marker, GetShaderLocation(gpu->Marker, "viewProjection"), viewProjection);
		SetShaderValueV(gpu->Marker, GetShaderLocation(gpu->Marker, "lightColors"), lightColorValues, SHADER_UNIFORM_VEC4, 4);
		DrawMeshInstanced(gpu->Sphere, gpu->MarkerMaterial, markers, 4);
		gpu->DrawCalls++;

		/* Fireflies last, made from nothing but their number: added to what is behind, tested against depth but not
		   writing it */
		if (gpu->ParticleCount > 0)
		{
			const Shader particle = gpu->Particle;
			const Vector4 params = {time, 0.8f + 2.2f * (1 - day), 0, 0};
			SetShaderValueMatrix(particle, GetShaderLocation(particle, "viewProjection"), viewProjection);
			SetVector(particle, "cameraRight", right, 0);
			SetVector(particle, "cameraUp", up, 0);
			SetShaderValue(particle, GetShaderLocation(particle, "params"), &params, SHADER_UNIFORM_VEC4);
			SetShaderValueV(particle, GetShaderLocation(particle, "lightColors"), lightColorValues, SHADER_UNIFORM_VEC4, 4);
			BeginBlendMode(BLEND_ADDITIVE);
			rlDisableDepthMask();
			rlDisableBackfaceCulling();
			rlEnableShader(particle.id);
			rlEnableVertexArray(gpu->EmptyVertexArray);
			rlDrawVertexArrayInstanced(0, 6, (int)gpu->ParticleCount);
			rlDisableVertexArray();
			rlDisableShader();
			rlEnableBackfaceCulling();
			rlEnableDepthMask();
			EndBlendMode();
			gpu->DrawCalls++;
		}

		rlDisableDepthTest();
		EndTextureMode();

		/* Bloom: what is brighter than white, at half size, blurred twice in each direction */
		const int bloomWidth = gpu->BloomA.texture.width;
		const int bloomHeight = gpu->BloomA.texture.height;

		if (gpu->Bloom)
		{
			const float threshold = gpu->Hdr ? 1.0f : 0.8f;
			SetShaderValue(gpu->Bright, GetShaderLocation(gpu->Bright, "threshold"), &threshold, SHADER_UNIFORM_FLOAT);
			PostPass(gpu, gpu->Bright, gpu->Scene.texture, &gpu->BloomA, bloomWidth, bloomHeight);
			const int texelAt = GetShaderLocation(gpu->Blur, "texel");

			for (int round = 0; round < 2; round++)
			{
				const Vector2 across = {1.0f / bloomWidth, 0};
				const Vector2 down = {0, 1.0f / bloomHeight};
				SetShaderValue(gpu->Blur, texelAt, &across, SHADER_UNIFORM_VEC2);
				PostPass(gpu, gpu->Blur, gpu->BloomA.texture, &gpu->BloomB, bloomWidth, bloomHeight);
				SetShaderValue(gpu->Blur, texelAt, &down, SHADER_UNIFORM_VEC2);
				PostPass(gpu, gpu->Blur, gpu->BloomB.texture, &gpu->BloomA, bloomWidth, bloomHeight);
			}
		}

		/* The HDR scene plus its glow, tone mapped to the screen */
		BeginDrawing();
		ClearBackground(BLACK);
		const float strength = gpu->Bloom ? 0.6f : 0.0f;
		SetShaderValue(gpu->Composite, GetShaderLocation(gpu->Composite, "strength"), &strength, SHADER_UNIFORM_FLOAT);
		BeginShaderMode(gpu->Composite);
		SetShaderValueTexture(gpu->Composite, GetShaderLocation(gpu->Composite, "bloom"), gpu->BloomA.texture);
		DrawTexturePro(gpu->Scene.texture, Rectangle{0, 0, (float)width, -(float)height}, Rectangle{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, Vector2{0, 0}, 0, WHITE);
		EndShaderMode();
		gpu->DrawCalls++;
		EndDrawing();
	}
}
