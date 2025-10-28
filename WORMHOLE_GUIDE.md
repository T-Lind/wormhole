# Wormhole Simulation - User Guide

## What You're Seeing

### The Wormhole Structure

The simulation visualizes a **Morris-Thorne traversable wormhole** - imagine a tunnel through space connecting two different regions (universes). Unlike black holes that trap everything, wormholes allow light and matter to pass through.

### Visual Elements

1. **Blue Glowing Ring**: The wormhole throat at l = 0 (the narrowest part)
2. **Colored Spheres**: Test objects showing which "universe" you're viewing
   - **Universe -1 (entrance)**: Red, Green, Blue, Orange spheres
   - **Universe +1 (exit)**: Yellow, Magenta, Cyan, White spheres
3. **Background Color**: 
   - Reddish tint = viewing from entrance side
   - Bluish tint = viewing from exit side

## Physics Explained

### The Morris-Thorne Metric

The spacetime curvature is described by:

```
ds² = -c²dt² + dl² + (b₀² + l²)(dθ² + sin²θ dφ²)
```

**What this means:**
- `l`: Position along the wormhole axis (negative = entrance, positive = exit)
- `b₀`: Throat radius (10 billion meters in this simulation)
- The shape function `r(l) = √(b₀² + l²)` creates a smooth throat

### How Light Travels

Light follows **null geodesics** - the straightest possible paths through curved spacetime. The geodesic equations are:

```
d²l/dλ² = -(l/(b₀² + l²)) × [(dr/dλ)² + r²(dφ/dλ)²]
d²r/dλ² = r(dφ/dλ)²
d²φ/dλ² = -2(dr/dλ)(dφ/dλ)/r
```

These are solved using **4th-order Runge-Kutta** integration for high accuracy.

## What to Look For

### Gravitational Lensing

As light passes near the throat, spacetime curvature bends its path. You should observe:

1. **Distortion**: Objects near the throat appear warped
2. **Light Bending**: Rays curve around the throat
3. **Multiple Images**: In some angles, you might see objects from the other universe
4. **Color Separation**: The throat glows blue, helping identify the tunnel

### Universe Transitions

When rays pass through the throat (crossing l = 0), they transition from universe -1 to universe +1 (or vice versa). This means:

- Standing on the entrance side, you can see exit-side objects **through** the wormhole
- The view through the throat is like looking through a telescope to another region of space

## Experimenting

### Try These Views

1. **Front View**: Start position - see the throat and surrounding spheres
2. **Side View**: Drag mouse horizontally - see the hourglass shape of spacetime
3. **Close-Up**: Scroll in - examine throat structure and light bending
4. **Far View**: Scroll out - see both universes and overall geometry

### Understanding Performance

- Each frame traces **480,000 rays** (800×600)
- Each ray can take up to **5,000 integration steps**
- Typical frame time: 0.2-5 seconds (CPU dependent)
- The simulation is accurate but computationally intensive

## Technical Details

### Coordinate Systems

The simulation uses **cylindrical coordinates** (l, ρ, φ) internally:
- `l`: Axial distance (wormhole axis)
- `ρ`: Radial distance from axis
- `φ`: Azimuthal angle

These are converted to Cartesian (x, y, z) for rendering.

### Integration Parameters

```cpp
MAX_STEPS = 5000        // Maximum geodesic steps
STEP_SIZE = 1e8         // 100,000 km per step
MAX_DISTANCE = 1e12     // Escape distance (1 million million km)
b0 = 1e10              // Throat radius (10 million km)
```

### Universe Detection

```cpp
if (l < -b0)  → universe = -1  (entrance)
if (l > +b0)  → universe = +1  (exit)
```

## Comparison with Black Holes

| Feature | Black Hole | Wormhole |
|---------|-----------|----------|
| Event Horizon | Yes (point of no return) | No |
| Light Escape | Cannot escape | Can pass through |
| Singularity | Yes (at center) | No |
| Metric | Schwarzschild | Morris-Thorne |
| Traversable | No | Yes (theoretically) |
| Requires Exotic Matter | No | Yes |

## Known Limitations

### Physical
- **No quantum effects**: Classical General Relativity only
- **No tidal forces**: Objects aren't stretched/compressed
- **Static geometry**: Throat size doesn't change
- **No exotic matter visualization**: We only show the geometry

### Computational
- **Simplified intersections**: Assumes small objects
- **No adaptive stepping**: Fixed step size
- **CPU only**: No GPU acceleration yet
- **No redshift**: Light color doesn't change with gravity

## Troubleshooting

### Simulation is slow
- **Normal**: Each frame requires millions of calculations
- **Expected FPS**: 0.2-1.0 frames per second
- Lower resolution: Edit `WIDTH` and `HEIGHT` in code

### Can't see objects
- **Zoom out**: Scroll wheel away from center
- **Rotate**: Left-drag to change viewing angle
- Objects might be behind you or through the wormhole

### Throat not visible
- The throat is at the origin (0, 0, 0)
- Look for the blue glowing ring
- Try zooming in closer

## Advanced Topics

### Exotic Matter Requirement

Real wormholes require **negative energy density** to stay open. This is:
- Not yet observed in nature
- Predicted by quantum field theory (Casimir effect)
- Required to prevent collapse
- The reason wormholes are theoretical

### Shape Function Analysis

The embedding diagram (if we could draw it) would show:
- Two asymptotically flat sheets (universes)
- Connected by a smooth throat
- Minimum radius = b₀
- No sharp edges or singularities

### Geodesic Classification

Light paths can be:
1. **Bound**: Spiral around throat, never escape
2. **Pass-through**: Enter from one side, exit the other
3. **Scattered**: Approach throat, bend away
4. **Absorbed**: Hit an object

## References for Further Study

### Foundational Papers
- Morris & Thorne (1988): Original wormhole paper
- Visser (1995): Comprehensive wormhole physics

### Books
- "Gravitation" by Misner, Thorne, Wheeler
- "Lorentzian Wormholes" by Matt Visser
- "Spacetime and Geometry" by Sean Carroll

### Online Resources
- [Einstein Online](http://www.einstein-online.info/)
- [arXiv General Relativity papers](https://arxiv.org/list/gr-qc/recent)

## Credits

Implementation based on:
- Morris-Thorne metric (1988)
- Numerical relativity techniques
- Ray tracing algorithms

Original black hole simulation: [YouTube Video](https://www.youtube.com/watch?v=8-B6ryuBkCM)

---

**Enjoy exploring curved spacetime!** 🌌

