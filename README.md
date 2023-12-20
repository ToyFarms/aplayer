# An Audio Player Written in C using FFMPEG

APlayer is an audio player written in C using mainly FFMPEG for reading audio file, and PortAudio to produce the sound.

It is developed for Windows, though most of the code are cross-platform. There are some Windows specific code lying around the place, it shouldn't be too tangled to factor out.

### Dependencies:
* [FFMPEG](https://github.com/FFmpeg/FFmpeg)
* [PortAudio](https://github.com/PortAudio/portaudio)
* [SDL2](https://github.com/libsdl-org/SDL)

### How to build:

Before proceeding, you will need:

* [msys64](https://www.msys2.org/) (For mingw (gcc, make, cmake) command)
* [vcpkg](https://github.com/microsoft/vcpkg) (For package manager)
* [git](https://git-scm.com/) (For cloning repo, or download manually)

Install dependencies.
```Shell
# install all the packages
vcpkg install ffmpeg
vcpkg install portaudio
vcpkg install sdl2

# clone the repo
git clone https://github.com/ToyFarms/aplayer [your_project]
cd [your_project]

# install the package to your project folder
vcpkg install
```

Now there should be a folder called `vcpkg_installed`. You just need to modify the `CMakeLists.txt` to match your `vcpkg_installed/[os]`.

All you need to do now is copy all the DLL and build the project.

```Shell
mkdir build
cd build

# copy all the DLL to ./build/
cp ../vcpkg_installed/[os]/bin/* .

# generate the Makefiles
cmake .. -G "Unix Makefiles"

# build the project
make

# run the application
./aplayer.exe
```