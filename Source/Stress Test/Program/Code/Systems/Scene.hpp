#pragma once

#include <Framework/Framework.hpp>

namespace Scene
{
	bool Load(Program::GpuState *gpu)
	{
		const unsigned int white = 0xFFFFFFFF;
		const unsigned int flat = 0xFFFF8080;
		gpu->White = LoadTextureFromImage(Image{(void *)&white, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
		gpu->FlatNormal = LoadTextureFromImage(Image{(void *)&flat, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
		gpu->Sphere = GenMeshSphere(1.0f, 12, 16);
		gpu->Plane = GenMeshPlane(200.0f, 200.0f, 1, 1);
		gpu->Starts = (Matrix *)MemAlloc(sizeof(Matrix) * Program::MAX_DUCKS);
		gpu->Animated = (Matrix *)MemAlloc(sizeof(Matrix) * Program::MAX_DUCKS);
		gpu->EmptyVertexArray = rlLoadVertexArray();
		return LoadDuck(gpu, "Models/Duck.glb") && MakeBricks(gpu, 1024) && gpu->Starts && gpu->Animated;
	}

	void Unload(Program::GpuState *gpu)
	{
		UnloadModel(gpu->Duck);
		UnloadMesh(gpu->Sphere);
		UnloadMesh(gpu->Plane);
		Texture2D textures[4] = {gpu->White, gpu->FlatNormal, gpu->BrickAlbedo, gpu->BrickNormal};

		for (Texture2D texture : textures)
		{
			UnloadTexture(texture);
		}

		/* The materials only point at textures unloaded above */
		Material *materials[4] = {&gpu->DuckMaterial, &gpu->PlaneMaterial, &gpu->MarkerMaterial, &gpu->ShadowMaterial};

		for (Material *material : materials)
		{
			MemFree(material->maps);
		}

		rlUnloadVertexArray(gpu->EmptyVertexArray);
		MemFree(gpu->Starts);
		MemFree(gpu->Animated);
	}

	/* The .glb's first mesh, which raylib has already placed by its node transforms, centered with its lowest point on the
	   ground */
	bool LoadDuck(Program::GpuState *gpu, const char *path)
	{
		gpu->Duck = Assets::LoadModel(path);

		if (gpu->Duck.meshCount < 1 || !gpu->Duck.meshes[0].vertices)
		{
			TraceLog(LOG_ERROR, "[Scene] Could not read %s", path);
			return false;
		}

		Mesh *mesh = &gpu->Duck.meshes[0];
		const BoundingBox box = GetMeshBoundingBox(*mesh);

		for (int v = 0; v < mesh->vertexCount; v++)
		{
			mesh->vertices[v * 3 + 0] -= (box.min.x + box.max.x) * 0.5f;
			mesh->vertices[v * 3 + 1] -= box.min.y;
			mesh->vertices[v * 3 + 2] -= (box.min.z + box.max.z) * 0.5f;
		}

		UpdateMeshBuffer(*mesh, 0, mesh->vertices, mesh->vertexCount * 3 * (int)sizeof(float), 0);

		const Texture2D albedo = gpu->Duck.materials[gpu->Duck.meshMaterial[0]].maps[MATERIAL_MAP_ALBEDO].texture;
		SetTextureFilter(albedo, TEXTURE_FILTER_BILINEAR);
		gpu->DuckMaterial = LoadMaterialDefault();
		gpu->DuckMaterial.maps[MATERIAL_MAP_ALBEDO].texture = albedo.id ? albedo : gpu->White;
		gpu->DuckMaterial.maps[MATERIAL_MAP_NORMAL].texture = gpu->FlatNormal;
		gpu->PlaneMaterial = LoadMaterialDefault();
		gpu->MarkerMaterial = LoadMaterialDefault();
		gpu->ShadowMaterial = LoadMaterialDefault();
		return true;
	}

	/* Where duck i starts, in a Matrix's first two columns (x, y, z, scale), (angle, spin, phase, unused): the first in
	   the middle, standing still, the rest on a spiral beyond the camera's circle */
	void PlaceDuck(unsigned int i, Matrix *duck)
	{
		const float angle = i * 2.39996f;
		const float radius = i ? sqrtf(64.0f + i * 0.45f) : 0.0f;
		*duck = Matrix{};
		duck->m0 = radius * cosf(angle);
		duck->m1 = 0;
		duck->m2 = radius * sinf(angle);
		duck->m3 = i ? 0.6f : 1.0f;
		duck->m4 = i ? -angle : 0.0f;
		duck->m5 = i ? 0.6f : 0.0f;
		duck->m6 = fmodf(i * 0.618034f, 1.0f) * 6.2831853f;
	}

	/* The same animation as AnimateDuck in Shaders/Common.glsli, on the CPU */
	void AnimateDuck(const Matrix *start, float time, Matrix *duck)
	{
		*duck = *start;

		if (start->m5 > 0)
		{
			const float s = sinf(time * 0.03f);
			const float c = cosf(time * 0.03f);
			duck->m0 = c * start->m0 - s * start->m2;
			duck->m1 = fabsf(sinf(time * 5.0f + start->m6)) * 0.3f * start->m3;
			duck->m2 = s * start->m0 + c * start->m2;
			duck->m4 = start->m4 + time * start->m5;
		}
	}

	static unsigned int Hash(unsigned int x, unsigned int y, unsigned int seed)
	{
		unsigned int h = x * 374761393u + y * 668265263u + seed * 2246822519u;
		h = (h ^ (h >> 13)) * 1274126177u;
		return h ^ (h >> 16);
	}

	static float Random(unsigned int x, unsigned int y, unsigned int seed)
	{
		return (Hash(x, y, seed) & 0xFFFFFF) / 16777215.0f;
	}

	/* Smooth noise that repeats every period cells, so the texture tiles */
	static float Noise(float x, float y, int period, unsigned int seed)
	{
		const int x0 = (int)floorf(x);
		const int y0 = (int)floorf(y);
		const float fx = x - x0;
		const float fy = y - y0;
		const float sx = fx * fx * (3 - 2 * fx);
		const float sy = fy * fy * (3 - 2 * fy);
		auto corner = [&](int cx, int cy) { return Random((unsigned int)((cx % period + period) % period), (unsigned int)((cy % period + period) % period), seed); };
		const float top = corner(x0, y0) + (corner(x0 + 1, y0) - corner(x0, y0)) * sx;
		const float bottom = corner(x0, y0 + 1) + (corner(x0 + 1, y0 + 1) - corner(x0, y0 + 1)) * sx;
		return top + (bottom - top) * sy;
	}

	float SmoothStep(float low, float high, float x)
	{
		const float t = Clamp((x - low) / (high - low), 0.0f, 1.0f);
		return t * t * (3 - 2 * t);
	}

	/* A texture from RGBA pixels, with mipmaps, trilinear and anisotropic filtering, repeating */
	static Texture2D MakeTexture(unsigned char *pixels, int size)
	{
		Texture2D texture = LoadTextureFromImage(Image{pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
		GenTextureMipmaps(&texture);
		SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
		SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_16X);
		SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
		return texture;
	}

	/* Procedural bricks with cracks: an albedo map and a normal map made from a height field, both tiling */
	bool MakeBricks(Program::GpuState *gpu, int size)
	{
		const int brickWidth = size / 4;
		const int brickHeight = size / 8;
		const float mortar = 6.0f;
		float *height = (float *)MemAlloc(sizeof(float) * size * size);
		float *crack = (float *)MemAlloc(sizeof(float) * size * size);
		unsigned char *albedo = (unsigned char *)MemAlloc(size * size * 4);
		unsigned char *normal = (unsigned char *)MemAlloc(size * size * 4);

		/* Cracks: random walks that wander and sometimes fork */
		for (int c = 0; c < 45; c++)
		{
			float x = Random(c, 1, 7) * size;
			float y = Random(c, 2, 7) * size;
			float direction = Random(c, 3, 7) * 2 * PI;
			const int length = 60 + (int)(Random(c, 4, 7) * 260);

			for (int step = 0; step < length; step++)
			{
				direction += (Random(c, step, 11) - 0.5f) * 0.45f;
				x += cosf(direction);
				y += sinf(direction);
				const float width = 2.0f * (1.0f - (float)step / length) + 0.6f;

				for (int dy = -3; dy <= 3; dy++)
				{
					for (int dx = -3; dx <= 3; dx++)
					{
						const int px = (((int)x + dx) % size + size) % size;
						const int py = (((int)y + dy) % size + size) % size;
						const float depth = Clamp(width - sqrtf((float)(dx * dx + dy * dy)) * 0.6f, 0.0f, 1.0f);
						crack[py * size + px] = fmaxf(crack[py * size + px], depth);
					}
				}
			}
		}

		for (int y = 0; y < size; y++)
		{
			const int row = y / brickHeight;
			const int shift = (row % 2) * brickWidth / 2;

			for (int x = 0; x < size; x++)
			{
				const int bx = (x + shift) % size;
				const int column = bx / brickWidth;
				const float localX = (float)(bx % brickWidth);
				const float localY = (float)(y % brickHeight);
				const float edge = fminf(fminf(localX, brickWidth - 1 - localX), fminf(localY, brickHeight - 1 - localY));
				const float brick = SmoothStep(mortar, mortar + 8.0f, edge);
				const float grain = Noise(x / 6.0f, y / 6.0f, size / 6, 3) * 0.6f + Noise(x / 1.5f, y / 1.5f, (int)(size / 1.5f), 5) * 0.4f;
				const float lumps = Noise(x / 24.0f, y / 24.0f, size / 24, 9);
				const float cracked = crack[y * size + x] * brick;
				height[y * size + x] = brick * (0.75f + 0.15f * lumps + 0.1f * grain) + (1 - brick) * 0.15f * grain - cracked * 0.65f;

				const float tone = Random(column, row, 13);
				const float brickColor[3] = {0.50f + 0.18f * tone, 0.20f + 0.08f * tone, 0.14f + 0.05f * tone};
				const float mortarColor[3] = {0.58f, 0.56f, 0.52f};
				const float shade = (0.8f + 0.25f * grain) * (1.0f - 0.75f * cracked);
				unsigned char *pixel = &albedo[(y * size + x) * 4];

				for (int channel = 0; channel < 3; channel++)
				{
					const float value = (brickColor[channel] * brick + mortarColor[channel] * (1 - brick)) * shade;
					pixel[channel] = (unsigned char)(Clamp(value, 0.0f, 1.0f) * 255);
				}

				pixel[3] = 255;
			}
		}

		/* The height field's slopes, as tangent-space normals (x along u, y along v) */
		const float strength = 5.0f;

		for (int y = 0; y < size; y++)
		{
			for (int x = 0; x < size; x++)
			{
				const float dx = height[y * size + (x + 1) % size] - height[y * size + (x + size - 1) % size];
				const float dy = height[((y + 1) % size) * size + x] - height[((y + size - 1) % size) * size + x];
				const Vector3 n = Vector3Normalize(Vector3{-dx * strength, -dy * strength, 1});
				unsigned char *pixel = &normal[(y * size + x) * 4];
				pixel[0] = (unsigned char)((n.x * 0.5f + 0.5f) * 255);
				pixel[1] = (unsigned char)((n.y * 0.5f + 0.5f) * 255);
				pixel[2] = (unsigned char)((n.z * 0.5f + 0.5f) * 255);
				pixel[3] = 255;
			}
		}

		gpu->BrickAlbedo = MakeTexture(albedo, size);
		gpu->BrickNormal = MakeTexture(normal, size);
		gpu->PlaneMaterial.maps[MATERIAL_MAP_ALBEDO].texture = gpu->BrickAlbedo;
		gpu->PlaneMaterial.maps[MATERIAL_MAP_NORMAL].texture = gpu->BrickNormal;
		MemFree(height);
		MemFree(crack);
		MemFree(albedo);
		MemFree(normal);
		return gpu->BrickAlbedo.id && gpu->BrickNormal.id;
	}
}
