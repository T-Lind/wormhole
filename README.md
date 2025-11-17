# wormhole

A real-time, interactive simulation of a traversable wormhole, rendered with a GPU compute shader.

The simulation places you in one of two distinct universes, each with its own sun and planetary system. The wormhole's throat acts as a window between them, distorting the view with a gravitational lensing effect.

The lensing is achieved by treating the wormhole's throat as a refractive sphere. When a view ray intersects the throat, its direction is bent based on its distance from the center, mimicking the path light would take through curved spacetime. The ray then continues into the other universe. This method produces a compelling visual distortion while keeping the simulation fast enough for real-time interaction.

### Building

You'll need a C++17 compiler, CMake, and vcpkg. Your graphics card must support OpenGL 4.3.

First, clone the repo, then run `vcpkg install` to get the dependencies (glew, glfw, glm).

After that, configure and build with CMake, pointing it to the vcpkg toolchain file.

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Running

The program is `WormholeSim.exe` inside the `build/Debug` directory. Running it starts the interactive mode.

**Controls:**
- `wasd`: Move forward/back/strafe
- `shift` / `space`: Move up / down
- `mouse drag`: Orbit camera
- `shift` + `mouse drag`: Pan camera
- `mouse scroll`: Zoom
- `u`: Switch universes
- `esc`: Quit

To render the cinematic video defined in `camera_path.txt`, run the program with the `-p` flag. It will save a video to the `exports` directory. This requires `ffmpeg` to be installed and in your system's PATH. If `ffmpeg` isn't found, the program will save the individual frames and print the command for you to run manually.
