Java 21 modified to work with Windows Vista. This build will work on unmodified Vista and networking (specifically in Minecraft) works now.

suggested build command is this or you will get missing function errors.

bash configure --with-target-bits=64 --with-toolchain-version=2019 --with-extra-cflags="-DPSAPI_VERSION=1"

you can also download a pre-compiled release in the releases section.

fair notice if you want to play minecraft 1.21+ LWJGL 3.3.3 is incompatible with stock Vista by default, You must use a launcher like "Prism Launcher" to replace the OpenAL.dll with the one from LWJGL 3.3.1.

If you need to contact me for any reason my discord server is your best bet.
My website: https://win-games.uk/
My discord: https://discord.gg/xZyz6WTfaT
