Java 21 modified to work with Windows Vista.
This build will work on unmodified Vista and networking (specifically in Minecraft) works now.

suggested build command is this or you will get missing function errors.

bash configure --with-target-bits=64 --with-toolchain-version=2019 --with-extra-cflags="-DPSAPI_VERSION=1"

you can also download a pre-compiled release in the releases section. 
