#include <Framework/Framework.hpp>

namespace Program
{
	namespace Lifecycle
	{
		/* Called once, when the program starts */
		DllFunction bool Startup(Program::State *state)
		{
			if (!state)
			{
				TraceLog(LOG_ERROR, "[Program][Lifecycle][Startup] state is null!");
				return false;
			}

			SetTraceLogLevel(LOG_WARNING);

			SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
			InitWindow((int)state->Resolution.x, (int)state->Resolution.y, state->WindowTitle);

			if (!IsWindowReady())
			{
				TraceLog(LOG_ERROR, "[Program][Lifecycle][Startup] Could not create the window");
				return false;
			}

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
			Image windowIcon = Assets::LoadIcon("Icon.png");
			if (IsImageValid(windowIcon))
			{
				SetWindowIcon(windowIcon);
			}

			UnloadImage(windowIcon);
#endif

			if (!Scene::Load(&state->Gpu))
			{
				return false;
			}

			InitAudioDevice();
			if (IsAudioDeviceReady())
			{
				state->Song = Assets::LoadMusicStream("Music/BlueMonday.ogg");
				state->MusicLoaded = IsMusicValid(state->Song);
				PlayMusicStream(state->Song);
			}

			TraceLog(LOG_WARNING, "Up/Down: ducks x2 or /2, Left/Right: fireflies /2 or x2, D: one draw per duck, S: shadows, A: animation (static, GPU vertex, CPU upload), "
			                      "B: bloom, V: VSync, M: mute. Phones: tap the right half for more ducks, the left half for fewer.");
			return true;
		}

		/* Called after Startup, and again whenever a new Program.dll is loaded: the shaders, so their edits show up */
		DllFunction bool Initialize(Program::State *state)
		{
			if (!Gpu::LoadShaders(&state->Gpu))
			{
				TraceLog(LOG_ERROR, "[Program][Lifecycle][Initialize] A shader failed to compile: see the errors above");
			}

			return true;
		}

		static void ChangeDucks(Program::State *state, bool more)
		{
			if (more && state->DuckCount < Program::MAX_DUCKS)
			{
				state->DuckCount *= 2;
			}
			else if (!more && state->DuckCount > 1)
			{
				state->DuckCount /= 2;
			}
		}

		/* Each key switches one part of the stress test (see the list Startup logs) */
		static void ReadInput(Program::State *state)
		{
			Program::GpuState *gpu = &state->Gpu;

			if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
			{
				ChangeDucks(state, true);
			}

			if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
			{
				ChangeDucks(state, false);
			}

			if (IsKeyPressed(KEY_RIGHT) && gpu->ParticleCount < 262144)
			{
				gpu->ParticleCount = gpu->ParticleCount ? gpu->ParticleCount * 2 : 1024;
			}

			if (IsKeyPressed(KEY_LEFT))
			{
				gpu->ParticleCount = gpu->ParticleCount > 1024 ? gpu->ParticleCount / 2 : 0;
			}

			if (IsKeyPressed(KEY_D))
			{
				gpu->InstancedDraws = !gpu->InstancedDraws;
			}

			if (IsKeyPressed(KEY_S))
			{
				gpu->Shadows = !gpu->Shadows;
			}

			if (IsKeyPressed(KEY_A))
			{
				gpu->AnimationMode = (Program::Animation)((gpu->AnimationMode + 1) % Program::ANIMATION_COUNT);
			}

			if (IsKeyPressed(KEY_B))
			{
				gpu->Bloom = !gpu->Bloom;
			}

			if (IsKeyPressed(KEY_V))
			{
				Gpu::ToggleVSync(gpu);
			}

			if (IsKeyPressed(KEY_M) && state->MusicLoaded)
			{
				state->Muted = !state->Muted;
				SetMusicVolume(state->Song, state->Muted ? 0.0f : 1.0f);
			}

			/* Taps (and clicks): the right half for twice the ducks, the left half for half */
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				ChangeDucks(state, GetMousePosition().x >= GetScreenWidth() * 0.5f);
			}
		}

		/* Called every frame */
		DllFunction bool Update(Program::State *state)
		{
			if (!state)
			{
				TraceLog(LOG_ERROR, "[Program][Lifecycle][Update] state is null!");
				return false;
			}

			const double deltaTime = GetFrameTime();
			state->Time += deltaTime;
			ReadInput(state);

			if (state->MusicLoaded)
			{
				UpdateMusicStream(state->Song);
			}

			if (!state->Gpu.ShadersFailed && GetRenderWidth() > 0 && GetRenderHeight() > 0)
			{
				Gpu::Frame(state);
			}
			else
			{
				BeginDrawing();
				ClearBackground(BLACK);
				DrawText(state->Gpu.ShadersFailed ? "A shader failed to compile: see the log." : "", 16, 16, 20, WHITE);
				EndDrawing();
			}

			/* Twice a second: frames per second and the load in the title */
			state->StatsFrames++;
			state->StatsTime += deltaTime;

			if (state->StatsTime >= 0.5)
			{
				const Program::GpuState *gpu = &state->Gpu;
				const double fps = state->StatsFrames / state->StatsTime;
				const double triangles = state->DuckCount * gpu->Duck.meshes[0].triangleCount / 1000.0;
				const char *animations[Program::ANIMATION_COUNT] = {"static", "GPU vertex", "CPU upload"};
				const char *title = String::Format("%u ducks (%.0fk tris) | %u fireflies | %u draws | shadows %s | bloom %s | %s | %s | VSync %s | %.0f fps (%.2f ms)",
				                                   state->DuckCount, triangles, gpu->ParticleCount, gpu->DrawCalls, gpu->Shadows ? "on" : "off", gpu->Bloom ? "on" : "off",
				                                   animations[gpu->AnimationMode], gpu->Hdr ? "HDR" : "LDR", gpu->VSync ? "on" : "off", fps, 1000.0 / fps);
				SetWindowTitle(title);
				TraceLog(LOG_WARNING, "%s", title);
				state->StatsFrames = 0;
				state->StatsTime = 0;
			}

			return true;
		}

		/* Called before Shutdown, and whenever Program.dll is about to be replaced */
		DllFunction void Deinitialize(Program::State *state)
		{
			Gpu::UnloadShaders(&state->Gpu);
		}

		/* Called once, when the program ends */
		DllFunction void Shutdown(Program::State *state)
		{
			if (state->MusicLoaded)
			{
				UnloadMusicStream(state->Song);
			}

			if (IsAudioDeviceReady())
			{
				CloseAudioDevice();
			}

			Gpu::Unload(&state->Gpu);
			Scene::Unload(&state->Gpu);

			if (IsWindowReady())
			{
				CloseWindow();
			}
		}
	}
}
