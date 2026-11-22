#pragma once

#include <raylib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef IS_DEBUG
    extern const unsigned char BakedPack[];
    extern const unsigned int BakedPackSize;
    extern const char BakedPackFile[];
    extern const unsigned int BakedPackHash;
    extern const unsigned long long BakedPackKey;
#endif

namespace Assets
{
    namespace Internal
    {
        constexpr int MaxPathLength = 1024;

        inline bool IsRelative(const char *path)
        {
            return path[0] != '\0' && path[0] != '/' && path[0] != '\\' && path[1] != ':';
        }

        // Where the packed assets are; a platform can define FRAMEWORK_ASSETS_BASE_PATH when the program's folder isn't it
        inline const char *BasePath()
        {
#if defined(FRAMEWORK_ASSETS_BASE_PATH)
            return FRAMEWORK_ASSETS_BASE_PATH;
#elif defined(__ANDROID__)
            return ""; // Inside the .apk, where raylib's fopen looks first
#else
            return GetApplicationDirectory();
#endif
        }

        inline unsigned char *ReadFile(const char *path, int *dataSize)
        {
            *dataSize = 0;
            FILE *file = fopen(path, "rb");

            if (!file)
            {
                return nullptr;
            }

            fseek(file, 0, SEEK_END);
            const long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            unsigned char *data = size > 0 ? (unsigned char *)MemAlloc((unsigned int)size) : nullptr;

            if (data && fread(data, 1, (size_t)size, file) == (size_t)size)
            {
                *dataSize = (int)size;
            }
            else if (data)
            {
                MemFree(data);
                data = nullptr;
            }

            fclose(file);
            return data;
        }

#ifdef IS_DEBUG

        inline bool DebugPath(const char *pathWithExtension, char *output)
        {
            snprintf(output, MaxPathLength, "%s../../../../../Program/Assets/%s", GetApplicationDirectory(), pathWithExtension);

            if (!IsRelative(pathWithExtension) || FileExists(output))
            {
                return IsRelative(pathWithExtension);
            }

            snprintf(output, MaxPathLength, "%s../../../../Cache/Assets/Debug/%s", GetApplicationDirectory(), pathWithExtension);
            return FileExists(output);
        }

#else

        inline unsigned long long Scramble(unsigned long long x)
        {
            x += 0x9E3779B97F4A7C15ull;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        }

        inline bool ReadPack(unsigned long long offset, void *data, unsigned int size)
        {
            bool read = BakedPackSize > 0;

            if (read)
            {
                memcpy(data, BakedPack + offset, size);
            }
            else
            {
                char path[MaxPathLength];
                snprintf(path, sizeof(path), "%s%s", BasePath(), BakedPackFile);
                FILE *file = fopen(path, "rb");
#ifdef _WIN32
                read = file && _fseeki64(file, (long long)offset, SEEK_SET) == 0 && fread(data, 1, size, file) == size;
#else
                read = file && fseeko(file, (off_t)offset, SEEK_SET) == 0 && fread(data, 1, size, file) == size;
#endif

                if (file)
                {
                    fclose(file);
                }
            }

            unsigned long long block = 0;

            for (unsigned long long i = offset; read && BakedPackKey && i < offset + size; i++)
            {
                block = i == offset || i % 8 == 0 ? Scramble(BakedPackKey + i / 8) : block;
                ((unsigned char *)data)[i - offset] ^= i < 12 ? 0 : (unsigned char)(block >> (i % 8 * 8));
            }

            return read;
        }

        inline const unsigned char *PackIndex()
        {
            static unsigned char *index = nullptr;
            static bool loaded = false;

            if (loaded)
            {
                return index;
            }

            loaded = true;
            unsigned int header[3] = {};

            if (ReadPack(0, header, sizeof(header)) && header[0] == 0x4B434150u && (index = (unsigned char *)MemAlloc((unsigned int)sizeof(header) + header[2])) && ReadPack(0, index, (unsigned int)sizeof(header) + header[2]))
            {
                unsigned int hash = 2166136261u;

                for (unsigned int i = 0; i < sizeof(header) + header[2]; i++)
                {
                    hash = (hash ^ index[i]) * 16777619u;
                }

                if (hash == BakedPackHash)
                {
                    return index;
                }
            }

            MemFree(index);
            index = nullptr;
            TraceLog(LOG_ERROR, "ASSETS: %s is missing, or is from another build of the game", BakedPackSize ? "The packed assets" : BakedPackFile);
            return nullptr;
        }

        inline bool FindPacked(const char *path, unsigned long long *offset, unsigned int *size)
        {
            const unsigned char *index = PackIndex();
            unsigned int header[3] = {};

            while (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))
            {
                path += 2;
            }

            if (index)
            {
                memcpy(header, index, sizeof(header));
            }

            for (const unsigned char *entry = index + sizeof(header); index && entry < index + sizeof(header) + header[2];)
            {
                unsigned int fields[4];
                memcpy(fields, entry, sizeof(fields));
                const char *name = (const char *)entry + sizeof(fields);
                entry += sizeof(fields) + fields[3];
                unsigned int i = 0;

                while (i < fields[3] && path[i] && (name[i] == '\\' ? '/' : tolower((unsigned char)name[i])) == (path[i] == '\\' ? '/' : tolower((unsigned char)path[i])))
                {
                    i++;
                }

                if (i == fields[3] && !path[i])
                {
                    *offset = sizeof(header) + header[2] + (fields[0] | (unsigned long long)fields[1] << 32);
                    *size = fields[2];
                    return true;
                }
            }

            return false;
        }

        inline unsigned char *ReadPacked(unsigned long long offset, unsigned int size)
        {
            unsigned char *data = (unsigned char *)MemAlloc(size);

            if (data && !ReadPack(offset, data, size))
            {
                MemFree(data);
                data = nullptr;
            }

            return data;
        }

#endif

        inline bool Exists(const char *path)
        {
#ifdef IS_DEBUG
            char assetPath[MaxPathLength];
            return DebugPath(path, assetPath);
#else
            unsigned long long offset = 0;
            unsigned int size = 0;
            return FindPacked(path, &offset, &size);
#endif
        }

        inline unsigned char *LoadFileData(const char *fileName, int *dataSize)
        {
            *dataSize = 0;

#ifdef IS_DEBUG
            char assetPath[MaxPathLength];

            if (DebugPath(fileName, assetPath))
            {
                return ReadFile(assetPath, dataSize);
            }
#else
            unsigned long long offset = 0;
            unsigned int size = 0;

            if (FindPacked(fileName, &offset, &size))
            {
                unsigned char *data = size ? ReadPacked(offset, size) : nullptr;
                *dataSize = data ? (int)size : 0;
                return data;
            }
#endif

            unsigned char *data = ReadFile(fileName, dataSize);

            if (!data)
            {
                TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
            }

            return data;
        }

        inline char *LoadFileText(const char *fileName)
        {
            int size = 0;
            unsigned char *data = LoadFileData(fileName, &size);
            char *text = data ? (char *)MemRealloc(data, (unsigned int)size + 1) : nullptr;

            if (!text)
            {
                MemFree(data);
                return nullptr;
            }

            text[size] = '\0';
            return text;
        }
    }

    inline void Initialize()
    {
        SetLoadFileDataCallback(Internal::LoadFileData);
        SetLoadFileTextCallback(Internal::LoadFileText);
    }

    inline Music LoadMusicStream(const char *fileName)
    {
#ifdef IS_DEBUG
        char assetPath[Internal::MaxPathLength];

        if (Internal::DebugPath(fileName, assetPath))
        {
            return ::LoadMusicStream(assetPath);
        }
#else
        static struct { unsigned long long Offset; unsigned char *Data; } kept[64];
        static int keptCount = 0;
        unsigned long long offset = 0;
        unsigned int size = 0;

        if (Internal::FindPacked(fileName, &offset, &size))
        {
            const unsigned char *data = BakedPackSize && !BakedPackKey ? BakedPack + offset : nullptr;

            for (int i = 0; !data && i < keptCount; i++)
            {
                data = kept[i].Offset == offset ? kept[i].Data : nullptr;
            }

            if (!data && (data = Internal::ReadPacked(offset, size)) && keptCount < 64)
            {
                kept[keptCount].Offset = offset;
                kept[keptCount++].Data = (unsigned char *)data;
            }

            char extension[16];
            snprintf(extension, sizeof(extension), "%s", GetFileExtension(fileName));

            for (char *cursor = extension; *cursor; cursor++)
            {
                *cursor = (char)tolower((unsigned char)*cursor);
            }

            return LoadMusicStreamFromMemory(extension, data, data ? (int)size : 0);
        }
#endif

        return ::LoadMusicStream(fileName);
    }

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    inline constexpr const char *ShaderFormatList[] = {".glsl300es", ".glsl330", ".glsl100", ".glsl120"};
#else
    inline constexpr const char *ShaderFormatList[] = {".glsl330", ".glsl300es", ".glsl100", ".glsl120"};
#endif

    namespace Internal
    {
        inline bool ResolveShader(const char *path, char *output)
        {
            if (!path)
            {
                return true;
            }

            char base[MaxPathLength];
            snprintf(base, sizeof(base), "%s", path);
            const size_t length = strlen(base);

            if (length > 5 && IsFileExtension(base, ".glsl"))
            {
                base[length - 5] = '\0';
            }

            for (const char *extension : ShaderFormatList)
            {
                snprintf(output, MaxPathLength, "%s%s", base, extension);

                if (Exists(output))
                {
                    return true;
                }
            }

            snprintf(output, MaxPathLength, "%s", path);
            return Exists(path) || ::FileExists(path);
        }

        // A shader's text. OpenGL ES 3 (web, Android) takes GLSL 330 as GLSL ES 300: the same language, plus precisions
        inline char *LoadShaderText(const char *path)
        {
            char *text = path ? ::LoadFileText(path) : nullptr;

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
            const char *version = text ? strstr(text, "#version") : nullptr;
            int number = 0;
            char profile[4] = "";

            if (!version || sscanf(version, "#version %d%*[ \t]%3[a-z]", &number, profile) < 1 || number < 300 || strcmp(profile, "es") == 0)
            {
                return text;
            }

            static const char header[] = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp samplerCube;\nprecision highp sampler3D;\nprecision highp sampler2DShadow;\nprecision highp sampler2DArray;\n";
            const char *rest = strchr(version, '\n');
            rest = rest ? rest + 1 : version + strlen(version);
            const size_t before = (size_t)(version - text);
            char *converted = (char *)MemAlloc((unsigned int)(before + sizeof(header) + strlen(rest)));

            if (converted)
            {
                memcpy(converted, text, before);
                memcpy(converted + before, header, sizeof(header) - 1);
                strcpy(converted + before + sizeof(header) - 1, rest);
            }

            UnloadFileText(text);
            return converted;
#else
            return text;
#endif
        }

        inline void Folder(const char *path, char *output)
        {
            snprintf(output, MaxPathLength, "%s", path);
            char *slash = strrchr(output, '/');
            char *backslash = strrchr(output, '\\');
            slash = backslash > slash ? backslash : slash;
            *(slash ? slash + 1 : output) = '\0';
        }

        inline void TemporaryPath(char *output, const char *extension)
        {
            static int counter = 0;
            const char *folder = getenv("TEMP") ? getenv("TEMP") : getenv("TMPDIR") ? getenv("TMPDIR") : ::DirectoryExists("/tmp") ? "/tmp" : ".";
            snprintf(output, MaxPathLength, "%s/Assets_%p_%d%s", folder, (void *)&counter, counter++, extension);
        }

        inline bool StartsWithWord(const char *line, const char *word)
        {
            const size_t length = strlen(word);
            return strncmp(line, word, length) == 0 && (line[length] == ' ' || line[length] == '\t');
        }

        inline bool WriteMaterials(const char *mtlPath, char *output)
        {
            static const char *maps[] = {"map_Ka", "map_Kd", "map_Ks", "map_Ns", "map_bump", "map_d", "bump", "disp"};
            char *text = LoadFileText(mtlPath);
            char folder[MaxPathLength];
            Folder(mtlPath, folder);
            TemporaryPath(output, ".mtl");
            FILE *file = text ? fopen(output, "wb") : nullptr;

            for (char *line = text; file && line && *line;)
            {
                char *next = strchr(line, '\n');
                next = next ? next + 1 : line + strlen(line);
                const char *word = line + strspn(line, " \t");
                const char *map = nullptr;

                for (const char *candidate : maps)
                {
                    map = !map && StartsWithWord(word, candidate) ? candidate : map;
                }

                const char *value = map ? word + strlen(map) + strspn(word + strlen(map), " \t") : nullptr;

                if (value && IsRelative(value))
                {
                    fprintf(file, "%s %s%.*s", map, folder, (int)(next - value), value);
                }
                else
                {
                    fwrite(line, 1, (size_t)(next - line), file);
                }

                line = next;
            }

            if (file)
            {
                fclose(file);
            }

            MemFree(text);
            return file != nullptr;
        }

        inline bool AssetFolder(const char *path, char *output)
        {
            snprintf(output, MaxPathLength, "%s", path);

            for (char *cursor = output; *cursor; cursor++)
            {
                *cursor = *cursor == '\\' ? '/' : *cursor;
            }

            while (output[0] == '.' && output[1] == '/')
            {
                memmove(output, output + 2, strlen(output + 2) + 1);
            }

            for (size_t length = strlen(output); length > 0 && output[length - 1] == '/'; length--)
            {
                output[length - 1] = '\0';
            }

            return IsRelative(path) || path[0] == '\0';
        }

        inline void AddPath(FilePathList *files, const char *path, int length)
        {
            for (unsigned int i = 0; i < files->count; i++)
            {
                if ((int)strlen(files->paths[i]) == length && strncmp(files->paths[i], path, (size_t)length) == 0)
                {
                    return;
                }
            }

            files->paths = (char **)MemRealloc(files->paths, (files->count + 1) * (unsigned int)sizeof(char *));
            files->paths[files->count] = (char *)MemAlloc((unsigned int)length + 1);
            memcpy(files->paths[files->count++], path, (size_t)length);
        }

#ifndef IS_DEBUG

        inline FilePathList ListPacked(const char *folder, const char *filter, bool scanSubdirs)
        {
            const bool allFiles = !filter || !filter[0] || strstr(filter, "*.*") || strstr(filter, "FILES*");
            const bool directories = filter && (strstr(filter, "*.*") || strstr(filter, "DIRS*"));
            const size_t length = strlen(folder);
            const unsigned char *index = PackIndex();
            unsigned int header[3] = {};
            FilePathList files = {};

            if (index)
            {
                memcpy(header, index, sizeof(header));
            }

            for (const unsigned char *entry = index + sizeof(header); index && entry < index + sizeof(header) + header[2];)
            {
                unsigned int fields[4];
                memcpy(fields, entry, sizeof(fields));
                const char *name = (const char *)entry + sizeof(fields);
                const char *end = name + fields[3];
                entry += sizeof(fields) + fields[3];
                size_t i = 0;

                while (i < length && i < fields[3] && tolower((unsigned char)name[i]) == tolower((unsigned char)folder[i]))
                {
                    i++;
                }

                if (i < length || fields[3] <= length || (length && name[length] != '/'))
                {
                    continue;
                }

                const char *rest = name + length + (length ? 1 : 0);
                const char *slash = (const char *)memchr(rest, '/', (size_t)(end - rest));

                for (const char *cut = slash; directories && cut; cut = scanSubdirs ? (const char *)memchr(cut + 1, '/', (size_t)(end - cut - 1)) : nullptr)
                {
                    AddPath(&files, name, (int)(cut - name));
                }

                char path[MaxPathLength];
                snprintf(path, sizeof(path), "%.*s", (int)fields[3], name);

                if ((!slash || scanSubdirs) && (allFiles || IsFileExtension(path, filter)))
                {
                    AddPath(&files, path, (int)fields[3]);
                }
            }

            return files;
        }

#endif
    }

    inline Shader LoadShader(const char *vertexPath, const char *fragmentPath)
    {
        char vertex[Internal::MaxPathLength];
        char fragment[Internal::MaxPathLength];

        if (!vertexPath && !fragmentPath)
        {
            return ::LoadShader(nullptr, nullptr);
        }

        if (Internal::ResolveShader(vertexPath, vertex) && Internal::ResolveShader(fragmentPath, fragment))
        {
            char *vertexText = Internal::LoadShaderText(vertexPath ? vertex : nullptr);
            char *fragmentText = Internal::LoadShaderText(fragmentPath ? fragment : nullptr);
            Shader shader = LoadShaderFromMemory(vertexText, fragmentText);
            UnloadFileText(vertexText);
            UnloadFileText(fragmentText);
            return shader;
        }

        TraceLog(LOG_ERROR, "SHADER: [%s, %s] No shader found. Was its .glsl compiled for this platform, or did it move?", vertexPath ? vertexPath : "default", fragmentPath ? fragmentPath : "default");
        return ::LoadShader(nullptr, nullptr);
    }

    inline Model LoadModel(const char *fileName)
    {
        char *text = IsFileExtension(fileName, ".obj") ? Internal::LoadFileText(fileName) : nullptr;
        char folder[Internal::MaxPathLength];
        char objPath[Internal::MaxPathLength];
        char mtlPath[Internal::MaxPathLength] = "";
        Internal::Folder(fileName, folder);
        Internal::TemporaryPath(objPath, ".obj");
        FILE *file = text ? fopen(objPath, "wb") : nullptr;

        for (char *line = text; file && *line;)
        {
            char *next = strchr(line, '\n');
            next = next ? next + 1 : line + strlen(line);
            const char *word = line + strspn(line, " \t");

            if (Internal::StartsWithWord(word, "mtllib"))
            {
                char path[Internal::MaxPathLength];
                const char *name = word + 6 + strspn(word + 6, " \t");
                snprintf(path, sizeof(path), "%s%.*s", folder, (int)strcspn(name, "\r\n"), name);

                if (Internal::WriteMaterials(path, mtlPath))
                {
                    fprintf(file, "mtllib %s\n", GetFileName(mtlPath));
                }
            }
            else
            {
                fwrite(line, 1, (size_t)(next - line), file);
            }

            line = next;
        }

        MemFree(text);

        if (!file)
        {
            return ::LoadModel(fileName);
        }

        fclose(file);
        Model model = ::LoadModel(objPath);
        remove(objPath);

        if (mtlPath[0])
        {
            remove(mtlPath);
        }

        return model;
    }

    inline Material *LoadMaterials(const char *fileName, int *materialCount)
    {
        char mtlPath[Internal::MaxPathLength];

        if (!IsFileExtension(fileName, ".mtl") || !Internal::WriteMaterials(fileName, mtlPath))
        {
            return ::LoadMaterials(fileName, materialCount);
        }

        Material *materials = ::LoadMaterials(mtlPath, materialCount);
        remove(mtlPath);
        return materials;
    }

    // An image for SetWindowIcon, in RGBA and enlarged by whole pixels to near 256x256: systems shrink it to the sizes
    // they show, which keeps pixel art sharp, where enlarging a small icon themselves would blur it
    inline Image LoadIcon(const char *fileName)
    {
        Image icon = LoadImage(fileName);
        const int largest = icon.width > icon.height ? icon.width : icon.height;
        const int scale = largest > 0 ? 256 / largest : 0;

        if (IsImageValid(icon))
        {
            ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        }

        if (scale >= 2)
        {
            ImageResizeNN(&icon, icon.width * scale, icon.height * scale);
        }

        return icon;
    }

    inline bool FileExists(const char *fileName)
    {
        return fileName && (Internal::Exists(fileName) || ::FileExists(fileName));
    }

    inline bool IsPathFile(const char *path)
    {
#ifdef IS_DEBUG
        char assetPath[Internal::MaxPathLength];

        if (Internal::DebugPath(path, assetPath))
        {
            return ::IsPathFile(assetPath);
        }
#else
        unsigned long long offset = 0;
        unsigned int size = 0;

        if (Internal::FindPacked(path, &offset, &size))
        {
            return true;
        }
#endif

        return ::IsPathFile(path);
    }

    inline int GetFileLength(const char *fileName)
    {
#ifdef IS_DEBUG
        char assetPath[Internal::MaxPathLength];

        if (Internal::DebugPath(fileName, assetPath))
        {
            return ::GetFileLength(assetPath);
        }
#else
        unsigned long long offset = 0;
        unsigned int size = 0;

        if (Internal::FindPacked(fileName, &offset, &size))
        {
            return (int)size;
        }
#endif

        return ::GetFileLength(fileName);
    }

    inline FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter, bool scanSubdirs)
    {
        char folder[Internal::MaxPathLength];
        FilePathList files = {};

        if (!Internal::AssetFolder(basePath, folder))
        {
            return ::LoadDirectoryFilesEx(basePath, filter, scanSubdirs);
        }

#ifdef IS_DEBUG
        char assetPath[Internal::MaxPathLength];
        snprintf(assetPath, sizeof(assetPath), "%s../../../../../Program/Assets/%s", GetApplicationDirectory(), folder);

        if (::DirectoryExists(assetPath))
        {
            files = ::LoadDirectoryFilesEx(assetPath, filter, scanSubdirs);

            for (unsigned int i = 0; i < files.count; i++)
            {
                char path[Internal::MaxPathLength];
                snprintf(path, sizeof(path), "%s%s", folder, files.paths[i] + strlen(assetPath) + (folder[0] ? 0 : 1));
                Internal::AssetFolder(path, files.paths[i]);
            }

            return files;
        }
#else
        files = Internal::ListPacked(folder, filter, scanSubdirs);

        if (files.count > 0)
        {
            return files;
        }
#endif

        return ::LoadDirectoryFilesEx(basePath, filter, scanSubdirs);
    }

    inline FilePathList LoadDirectoryFiles(const char *dirPath)
    {
        return LoadDirectoryFilesEx(dirPath, "*.*", false);
    }

    inline bool DirectoryExists(const char *dirPath)
    {
        char folder[Internal::MaxPathLength];

        if (!Internal::AssetFolder(dirPath, folder))
        {
            return ::DirectoryExists(dirPath);
        }

#ifdef IS_DEBUG
        char assetPath[Internal::MaxPathLength];
        snprintf(assetPath, sizeof(assetPath), "%s../../../../../Program/Assets/%s", GetApplicationDirectory(), folder);
        return ::DirectoryExists(assetPath) || ::DirectoryExists(dirPath);
#else
        FilePathList files = Internal::ListPacked(folder, "*.*", false);
        UnloadDirectoryFiles(files);
        return files.count > 0 || ::DirectoryExists(dirPath);
#endif
    }

    namespace Assets = ::Assets;
}
