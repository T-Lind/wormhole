# wormhole

This is a real-time simulation of a traversable wormhole. It's all running on the gpu with a compute shader, which makes it fast enough to be interactive.

The whole idea is to show what it might look like to approach a morris-thorne wormhole, which is basically a tunnel connecting two different places in spacetime. I've set up two unique little universes, each with some planets and a sun. You can see from one universe into the other through the wormhole's throat, and the view is distorted by a gravitational lensing effect.

It's not perfectly, scientifically accurate. the lensing is a visual approximation (using a refraction effect in the shader) instead of a full general relativity calculation. This is a tradeoff to get it running in real-time.

### Building the project

You'll need a c++17 compiler, cmake, and vcpkg. your graphics card should also support opengl 4.3.

First, clone the repo. then, run `vcpkg install` to get the dependencies.

After that, you can configure and build with cmake. you just need to point it to the vcpkg toolchain file.

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Running it

The main program is `WormholeSim` inside the `build` directory.

Running it by itself starts the interactive mode. you can fly around with the mouse and keyboard.

Controls:
wasd: move around
shift / space: move up / down
mouse drag: orbit camera
shift + mouse drag: pan camera
mouse scroll: zoom
u: switch universes
esc: quit

If you want to render the cinematic video, run it with the `-p` flag. It will use the `camera_path.txt` file to create a video in the `exports` directory. This needs ffmpeg to be installed on your system to create the mp4 automatically. If it's not, the program will just save all the frames as images and tell you the command to stitch them together yourself.

### The physically accurate renderer

This project also includes a second program, `WormholeGeodesic`.

Unlike the main simulation, which uses a fast visual approximation for lensing, this program is a non-interactive, cpu-based renderer that calculates the actual curved path of light by numerically solving the geodesic equations of general relativity.

It is extremely slow but produces a physically accurate image. it renders a single frame from a fixed camera position and saves it to the `exports` directory.

It's still a buggy WIP, sorry.
