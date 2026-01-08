Java 21 modified to work with Windows Vista/XP. This build will work on unmodified Vista/XP and networking (specifically in Minecraft*) works now.

Suggested 64-bit build command (PSAPI_VERSION=1 is required or you will get missing function errors):

```bash configure --with-target-bits=64 --with-toolchain-version=2017 --with-extra-cxxflags="-DPSAPI_VERSION=1" --with-extra-cflags="-DPSAPI_VERSION=1"```

You can also download a pre-compiled release in the releases section.

fair notice: If you want to play Minecraft 1.21+, LWJGL 3.3.3 is incompatible with stock Vista by default. You can use a launcher like "Prism Launcher" to replace the OpenAL.dll with the one from LWJGL 3.3.1, or apply this Java argument to your current launcher:

```-Dorg.lwjgl.openal.libname=C:\YourFilePathWithTheOpenALFileGoesHere\OpenAL.dll```

Another thing, this is targetting unmodified Windows Vista. the Vista extended kernel could cause issues with this that would otherwise not happen on a stock Vista system. 
Compatibility is not guaranteed with the extended kernel installed.

------------------------------------
*SPECIFIC INFORMATION TO XP

Compiling for Windows XP is a bit complicated and hacky.<br>
After compiling, You must patch the subsystem version to 5.0 for x86 or 5.2 for x64 instead of the default 6.0 subsystem post build.<br>
You must also obtain a UCRTBASE.dll that is compatible with XP post build.<br>
The release builds already have this work completed.<br>

<br>

It's also worth mentioning that Minecraft 1.21 on XP is not stable due to LWJGL native dlls breaking on XP.<br>
Older OpenAL alone is not enough on XP.<br>
There may be a workaround to get it launched (I was able to get it to work sometimes with ```-Dorg.lwjgl.system.allocator=system```) It is not stable.

------------------------------------

If you need to contact me for any reason, my Discord server is your best bet.

- **My website**: https://win-games.uk/
- **My Discord**: https://discord.gg/KMAq2mVaXp
- **My email**: windvd@win-games.uk
