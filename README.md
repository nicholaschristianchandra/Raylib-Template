# Raylib Template
This is my personal Raylib template which runs on windows, ideally on Visual Studio IDE.
Some warning: a lot of this template is vibe-coded. Why? Because I want to focus on actually programming and making these kinds of stuff are extremely fragile and filled with gotchas. I've test, revised, and fine tuned heavily the outputted template for the last 6 months, and I am confident to say that it's stable.

It includes features such as...

## Custom incremental build system
By default it supports builds to these platforms:
- **Windows**: supports both x64 and x86, with an option to make it a single exe using EMBED_ASSETS and compression using UPX
- **Linux**: exports to Linux via Zig which can be packaged as a tar.gz or AppImage. Like windows, it can also be single executable and compressed.
- **Web**: exports to web via Emscripten.
- **Android**: exports to APK and/or AAB without a gradle project nor Android Studio. It also allows you to set up your keystore easily during setup. Also supports USB debugging.

## Hot reload
Easily iterate your programming process using hot reload. (Debug only, not available on Release)
Any code you wrote will be reloaded via a .dll and any data put inside Program/Types/Program's Program State will be preserved upon the next reload.

## Asset Pipeline
Importing assets (images, audio, glsl, and really anything) is as simple as putting it under Program/Assets.
All files inside that folder will be put inside Assets.data which will be embedded/separated from the exe depending on EMBED_ASSETS.
Assets.data can also be encrypted using the ENABLE_ENCRYPTION option so it could not be pulled with ordinary tools.

Using an asset with raylib is as simple as using the load functions inside the Assets:: namespace which will load from the root of Program/Assets folder.
```cpp
Texture2D texture = LoadTexture("Sprites/Player.png");
Sound sound = LoadSound("Audio/Jump.wav");
Music music = Assets::LoadMusicStream("Audio/Theme.ogg");
Font font = LoadFont("Fonts/Main.ttf");
Model model = Assets::LoadModel("Models/Hero.glb");
char *text = LoadFileText("Datas/Dialogue.txt");
Image icon = Assets::LoadIcon("Icon.png");
```

## Automatic header generation
Hate headers? Me too. Stop writing headers and start writing actual code using the template's custom C++ parser.
Behind the scenes, it will recreate a header that will extern all of your functions and variables. 
A function or variable could be marked as private by putting it inside an empty namespace or with the static keyword.

The Systems/Types/Datas framework split exists for this very reason. More on that in the Custom framework section.
Of course, like any custom parser it has limitation, but it has been tested and iterated and for 90% of your usage, as long as you don't do very weird and niche C++, you'll be fine.

## .dmp error reports
Whenever your program crashes, it will create a .dmp under %localappdata%/[Program Name] which you can drag and drop to visual studio to inspect its cause.

## Utilities
A tiny utility script I wrote because I personally love doing C-like C++ and need some kind of type/macro and utilities.
- **String**: Basically a char array with utilities to process it (e.g: ToUpper(), Length(), Contains(), etc)
- **Array**: My custom fixed array type which supports empty elements and has list-like utilities (e.g: Reverse(), FirstIndex(), RemoveAll(), etc)
- **CArray**: This is basically just a utility for normal C arrays. Because CArray and Array are two different types. (e.g: Length(), SetFromTo(), SetAll(), etc)

## Custom Framework
All game code lives in one of three folders under Program\Code . If you know C++ projects: Types are the headers and Systems are the source files, except that you never write a declaration twice.
- **Types**: What every script needs whole: structs, classes, enums, unions, templates, constants, and inline or constexpr functions. Types files are included in every script, so a function defined in one must be inline, constexpr or a template. A Types file that uses another includes it: #include <Program/Code/Types/Other.hpp>.
- **Datas**: Data definitions: databases, settings and similar tables.
- **Systems**: Logic. Any script can use another script's functions and variables by name, with no includes and in any order.

Every script must start with
```cpp
#pragma once
#include <Framework/Framework.hpp>
```
For all the magic to work.
