# wormhole simulation

this is a real-time simulation of a traversable wormhole. it's all running on the gpu with a compute shader, which makes it fast enough to be interactive.

the whole idea is to show what it might look like to approach a morris-thorne wormhole, which is basically a tunnel connecting two different places in spacetime. i've set up two unique little universes, each with some planets orbiting a sun. you can see from one universe into the other through the wormhole's throat, and the view is distorted by a gravitational lensing effect.

it's not perfectly, scientifically accurate. the lensing is a visual approximation (using a refraction effect in the shader) instead of a full general relativity calculation. this is a tradeoff to get it running in real-time. the physics of the planets are also just simple animations. but it looks cool, and that's the main point.

### building the project

you'll need a c++17 compiler, cmake, and vcpkg. your graphics card should also support opengl 4.3.

first, clone the repo. then, run `vcpkg install` to get the dependencies.

after that, you can configure and build with cmake. you just need to point it to the vcpkg toolchain file.

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### running it

the main program is `WormholeSim` inside the `build` directory.

running it by itself starts the interactive mode. you can fly around with the mouse and keyboard.

**controls:**
wasd: move around
shift / space: move up / down
mouse drag: orbit camera
shift + mouse drag: pan camera
mouse scroll: zoom
u: switch universes
esc: quit

if you want to render the cinematic video, run it with the `-p` flag. it will use the `camera_path.txt` file to create a video in the `exports` directory. this needs ffmpeg to be installed on your system to create the mp4 automatically. if it's not, the program will just save all the frames as images and tell you the command to stitch them together yourself.
