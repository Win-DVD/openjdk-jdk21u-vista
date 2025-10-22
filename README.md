Java 21 modified to work with Windows Vista. This build will work on unmodified Vista and networking (specifically in Minecraft) works now.

Suggested build command (or you will get missing function errors):

```bash configure --with-target-bits=64 --with-toolchain-version=2019 --with-extra-cflags="-DPSAPI_VERSION=1"```

You can also download a pre-compiled release in the releases section.

fair notice: If you want to play Minecraft 1.21+, LWJGL 3.3.3 is incompatible with stock Vista by default. You can use a launcher like "Prism Launcher" to replace the OpenAL.dll with the one from LWJGL 3.3.1, or apply this Java argument to your current launcher:

```-Dorg.lwjgl.openal.libname=C:\YourFilePathWithTheOpenALFileGoesHere\OpenAL.dll```

If you need to contact me for any reason, my Discord server is your best bet.

- **My website**: https://win-games.uk/
- **My Discord**: https://discord.gg/xZyz6WTfaT
- **My email**: windvd@win-games.uk
