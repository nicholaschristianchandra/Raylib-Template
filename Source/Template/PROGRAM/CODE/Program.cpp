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

#ifndef __EMSCRIPTEN__
			SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
#endif

			InitAudioDevice();

			state->Screen = LoadRenderTexture((int)state->Resolution.x, (int)state->Resolution.y);
			SetTextureFilter(state->Screen.texture, TEXTURE_FILTER_POINT);

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
			Image windowIcon = Assets::LoadIcon("Icon.png");
			if (IsImageValid(windowIcon))
			{
				SetWindowIcon(windowIcon);
				UnloadImage(windowIcon);
			}
			else
			{
				TraceLog(LOG_WARNING, "[Program][Lifecycle][Startup] No window icon");
			}
#endif

			return true;
		}

		/* Called after Startup, and again whenever a new Program.dll is loaded */
		DllFunction bool Initialize(Program::State *state)
		{
			return true;
		}

		/* Called every frame */
		DllFunction bool Update(Program::State *state)
		{
			if (!state)
			{
				TraceLog(LOG_ERROR, "[Program][Lifecycle][Update] state is null!");
				return false;
			}

			BeginTextureMode(state->Screen);
			ClearBackground(BLACK);

			const char *text = state->WindowTitle;
			const int fontSize = 30;
			DrawText(text, ((int)state->Resolution.x - MeasureText(text, fontSize)) / 2, ((int)state->Resolution.y - fontSize) / 2, fontSize, WHITE);

			EndTextureMode();

			const float scaleX = GetScreenWidth() / state->Resolution.x;
			const float scaleY = GetScreenHeight() / state->Resolution.y;
			const float scale = scaleX < scaleY ? scaleX : scaleY;
			const Rectangle screen = {(GetScreenWidth() - state->Resolution.x * scale) / 2.0f, (GetScreenHeight() - state->Resolution.y * scale) / 2.0f, state->Resolution.x * scale, state->Resolution.y * scale};
			SetMouseOffset((int)-screen.x, (int)-screen.y);
			SetMouseScale(1.0f / scale, 1.0f / scale);

			BeginDrawing();
			ClearBackground(BLACK);
			DrawTexturePro(state->Screen.texture, Rectangle{0, 0, state->Resolution.x, -state->Resolution.y}, screen, Vector2{0, 0}, 0.0f, WHITE);
			EndDrawing();

			return true;
		}

		/* Called before Shutdown, and whenever Program.dll is about to be replaced */
		DllFunction void Deinitialize(Program::State *state)
		{
		}

		/* Called once, when the program ends */
		DllFunction void Shutdown(Program::State *state)
		{
			UnloadRenderTexture(state->Screen);

			if (IsAudioDeviceReady())
			{
				CloseAudioDevice();
			}

			if (IsWindowReady())
			{
				CloseWindow();
			}
		}
	}
}
