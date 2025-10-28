# **Wormhole** Simulation

A physically-accurate wormhole simulation using General Relativity and ray tracing techniques.

## Overview

This project simulates a **Morris-Thorne traversable wormhole** - a theoretical tunnel through spacetime that connects two distant regions. Unlike black holes which trap light, wormholes allow light (and matter) to pass through from one side to the other, creating unique visual effects through gravitational lensing.

## Physics & Mathematics

### Morris-Thorne Wormhole Metric

The spacetime geometry is described by the Morris-Thorne metric:

```
ds² = -c²dt² + dl² + (b₀² + l²)(dθ² + sin²θ dφ²)
```

Where:
- **l**: Proper radial distance along the wormhole axis
- **b₀**: Throat radius (minimum radius of the wormhole)
- **θ, φ**: Angular coordinates (spherical)
- **c**: Speed of light

### Shape Function

The radial coordinate as a function of proper distance:

```
r(l) = √(b₀² + l²)
```

This creates a smooth, symmetric throat connecting two asymptotically flat spacetime regions.

### Geodesic Equations

Null geodesics (light paths) are computed by solving:

```
d²l/dλ² = -(l/(b₀² + l²)) · (dr/dλ)² + r²(dφ/dλ)²)

d²r/dλ² = r(dφ/dλ)²

d²φ/dλ² = -2(dr/dλ)(dφ/dλ)/r
```

Where **λ** is an affine parameter along the geodesic.

### Key Properties

1. **No Event Horizon**: Unlike black holes, wormholes have no point of no return
2. **Traversable**: Light and matter can pass through
3. **Requires Exotic Matter**: Theoretical stability requires negative energy density (not yet observed)
4. **Two Asymptotic Regions**: Each end approaches flat spacetime at infinity

## Implementation Details

### Ray Tracing Algorithm

1. **Cast rays** from camera through each pixel
2. **Integrate geodesics** using RK4 (Runge-Kutta 4th order) method
3. **Track universe state**: rays transition between "entrance" and "exit" sides
4. **Check intersections** with scene objects in current universe
5. **Render** with physically-based shading

### Two-Universe Scene

The simulation creates two distinct "universes" on either side:
- **Universe -1 (Entrance)**: Red, Green, Blue spheres
- **Universe +1 (Exit)**: Yellow, Magenta, Cyan spheres

Objects are only visible from their respective universe unless viewed through the wormhole throat.

## Building Requirements

1. C++ Compiler supporting C++17 or newer
2. [CMake](https://cmake.org/) (3.21+)
3. [Vcpkg](https://vcpkg.io/en/)
4. [Git](https://git-scm.com/)

## Build Instructions

### Using Vcpkg (Recommended)

1. Clone the repository:
```bash
git clone https://github.com/kavan010/wormhole.git
cd wormhole
```

2. Install dependencies with Vcpkg:
```bash
vcpkg install
```

3. Get the vcpkg cmake toolchain file path:
```bash
vcpkg integrate install
```
This will output something like:
```
CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

4. Create a build directory:
```bash
mkdir build
```

5. Configure project with CMake:
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```
Use the vcpkg cmake toolchain path from step 3.

6. Build the project:
```bash
cmake --build build
```

7. Run the wormhole simulation:
```bash
./build/WormholeSim       # Linux/Mac
.\build\Debug\WormholeSim.exe  # Windows
```

### Alternative: Debian/Ubuntu apt

If you don't want to use vcpkg:

```bash
sudo apt update
sudo apt install build-essential cmake \
    libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev
```

Then run:
```bash
cmake -B build -S .
cmake --build build
```

## Usage

### Controls

- **Left Mouse Drag**: Rotate camera around wormhole
- **Mouse Scroll**: Zoom in/out
- **ESC**: Exit simulation

### What You'll See

From the entrance side, you can see:
1. Colored spheres, suns, and custom loaded models in your local universe
2. The wormhole throat (dark region at center)
3. Distorted view of the exit universe through the throat
4. Gravitational lensing effects around the throat

## Executables

After building, you'll have three executables:

- **WormholeSim**: Main wormhole simulation with GPU-accelerated ray tracing. Supports interactive and cinematic movie modes.
- **Wormhole3D**: Legacy black hole with accretion disk
- **Wormhole2D**: 2D gravitational lensing demo

### New Features
- **GPU Accelerated Ray Tracing**: Uses a compute shader to leverage the power of the GPU for massive performance gains.
- **Cinematic Movie Mode**: Automatically generates a video from a predefined camera path, saving timestamped renders and the final `.mp4` video to the `exports/` directory.
- **Real-time Interactive Mode**: Fly through the scene with WASD controls and mouse-look, with a real-time FPS counter in the window title.
- **OBJ Model Loading**: Load custom 3D models in the `.obj` format into the simulation. A sample `spaceship.obj` is included.
- **Enhanced Visuals**: The simulation now includes procedural suns and a multi-colored, variable-size starfield for a more immersive experience.

## Technical Details

### Code Structure

- `wormhole_sim.cpp`: Main wormhole simulation
  - Controls CPU-side logic, scene setup, and input.
  - Manages GPU buffers and dispatches the compute shader.
- `wormhole.comp`: GPU compute shader
  - Performs all ray tracing, intersection tests (sphere, cube, triangle), and lighting calculations in parallel.
- `tiny_obj_loader.h`: A header-only library used for loading `.obj` models.
- `wormhole.cpp`: Legacy black hole with compute shaders
- `2D_lensing.cpp`: 2D Schwarzschild lensing
- `geodesic.comp`: GPU-accelerated geodesic computation

### Performance

- CPU-based ray tracing for maximum accuracy
- RK4 integration with adaptive step sizes
- Parallel ray tracing (OpenMP support)
- Typical: 1-5 FPS at 800x600 (depends on geodesic complexity)

### Parameters (adjustable in code)

```cpp
const double b0 = 1e10;        // Throat radius (10 million km)
const int MAX_STEPS = 5000;    // Max geodesic integration steps
const double STEP_SIZE = 1e8;  // Integration step (100,000 km)
```

## Scientific Accuracy

This simulation uses:
- ✅ Exact Morris-Thorne metric
- ✅ Null geodesic equations from General Relativity
- ✅ 4th-order Runge-Kutta integration
- ✅ Proper coordinate transformations

Limitations:
- ⚠️ **Visual Approximation of Lensing**: The gravitational lensing is achieved through a simplified refraction effect in the shader, not a full integration of null geodesics for the Morris-Thorne metric. This provides a visually compelling and fast approximation but is not physically rigorous.
- ⚠️ Simplified intersection tests (assumes objects are small)
- ⚠️ No gravitational redshift/blueshift
- ⚠️ No Doppler effects from motion
- ⚠️ Ignores quantum effects at throat

## References

### Papers
1. Morris, M. S., & Thorne, K. S. (1988). "Wormholes in spacetime and their use for interstellar travel: A tool for teaching general relativity." *American Journal of Physics*, 56(5), 395-412.

2. Visser, M. (1995). *Lorentzian Wormholes: From Einstein to Hawking*. AIP Press.

### Books
- Misner, C. W., Thorne, K. S., & Wheeler, J. A. (1973). *Gravitation*. W. H. Freeman.
- Carroll, S. M. (2004). *Spacetime and Geometry: An Introduction to General Relativity*. Addison Wesley.

## Future Enhancements

Potential improvements:
- [ ] GPU acceleration with compute shaders
- [ ] Gravitational redshift/blueshift
- [ ] Accretion effects at throat
- [ ] Dynamic throat radius
- [ ] Rotating (Kerr-like) wormholes
- [ ] Real-time performance optimization

## License

This project is open source. See the original black hole simulation: https://www.youtube.com/watch?v=8-B6ryuBkCM

## Contributing

Feel free to open issues or submit pull requests with improvements!

## Acknowledgments

Built upon the original black hole simulation framework. Extended with wormhole physics based on the pioneering work of Morris, Thorne, and Visser.

## Running the Simulation

The project is built using CMake. Once compiled, you can run the simulation from the project's root directory.

### Executable

The main executable is `WormholeSim.exe` (located in `build/Release/`).

### Modes

*   **Interactive Mode (Default)**: Simply run the executable to start in interactive mode.
    ```bash
    ./build/Release/WormholeSim.exe
    ```
*   **Predefined Path Mode**: Use the `-p` or `--predefined` flag to run the cinematic movie mode. This will render a video file (`.mp4`) to the `exports/` directory.
    ```bash
    ./build/Release/WormholeSim.exe -p
    ```

### Controls (Interactive Mode)

*   **Left Mouse Drag**: Orbit camera
*   **Shift + Left Drag**: Pan camera
*   **Mouse Scroll**: Zoom in/out
*   **W/A/S/D**: Move camera forward/left/back/right
*   **Shift**: Move camera up
*   **Space**: Move camera down
*   **U**: Switch between Universe 1 and Universe 2
*   **ESC**: Exit the simulation

## Sharing with Friends

To share the simulation, you can send them the `WormholeSim_dist.zip` file.

### Contents of the Package

*   `WormholeSim.exe`: The main application.
*   `wormhole.comp`: The required GPU shader file.
*   `camera_path.txt`: The camera path for the predefined movie mode.

**Important**: All three files must be kept in the same folder for the simulation to work correctly.

### System Requirements

*   A modern, OpenGL 4.3 compatible graphics card (most GPUs from the last ~8 years).
*   Windows operating system.

---

This project was developed as a demonstration of advanced real-time graphics techniques.
