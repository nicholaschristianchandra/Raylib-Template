#pragma once

#include <Framework/Framework.hpp>

namespace Program
{
	constexpr unsigned int WINDOW_TITLE_MAX_LENGTH = 128;

	struct State
	{
		String(WINDOW_TITLE_MAX_LENGTH) WindowTitle = { PROGRAM_NAME };
		Vector2 Resolution = {.x = 640, .y = 480};
		RenderTexture2D Screen = {};
	};
}
