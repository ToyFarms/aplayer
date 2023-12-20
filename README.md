# An Audio Player Written in C using FFMPEG

APlayer is an audio player written in C using mainly FFMPEG for reading audio file, and PortAudio to produce the sound.

It is developed for Windows, though most of the code are cross-platform. There are some Windows specific code lying around the place, it should not be too tangled to factor out.

### Dependencies:
* [FFMPEG](https://github.com/FFmpeg/FFmpeg)
* [PortAudio](https://github.com/PortAudio/portaudio)
* [SDL2](https://github.com/libsdl-org/SDL)

### How to build:

Install dependencies. For simplicity, we will use [vcpkg](https://github.com/microsoft/vcpkg).
```Shell
vcpkg install ffmpeg
vcpkg install portaudio
vcpkg install sdl2

cd [your_project]
vcpkg new
vcpkg add ffmpeg
vcpkg add portaudio
vcpkg add sdl2
vcpkg install
```

Now there should be a folder called `vcpkg_installed`. You just need to modify the `CMakeLists.txt` to match your `vcpkg_installed/[os]`.

All you need to do now is copy all the DLL and build the project.

```Shell
mkdir build
cd build

cp ../vcpkg_installekd/[os]/bin/* .

cmake .. -G "Unix Makefiles"
make
./aplayer.exe
```