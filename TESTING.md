# Wormhole Simulation - Testing & Verification Guide

## Quick Start Test

### 1. Build Verification

```bash
cd wormhole
cmake --build build --target WormholeSim
```

**Expected Output:**
```
Building Custom Rule...
wormhole_sim.cpp
WormholeSim.vcxproj -> .../build/Debug/WormholeSim.exe
```

✅ **Success**: Executable created without errors
❌ **Failure**: Check compiler C++17 support and dependencies

### 2. Launch Test

```bash
# Linux/Mac
./build/WormholeSim

# Windows
.\build\Debug\WormholeSim.exe
```

**Expected Console Output:**
```
╔═══════════════════════════════════════════════════════════════╗
║          MORRIS-THORNE WORMHOLE SIMULATION                    ║
╚═══════════════════════════════════════════════════════════════╝

Physics Parameters:
  Throat Radius (b₀):  10 thousand km
  Metric:              Morris-Thorne (traversable)
  Integration:         RK4 (4th-order Runge-Kutta)
  ...
```

✅ **Success**: Window opens, rendering begins
❌ **Failure**: Check OpenGL drivers and GLFW installation

## Visual Verification Tests

### Test 1: Initial View
**What you should see:**
- Window opens at 800×600 pixels
- Black/dark background
- Blue glowing ring at center (wormhole throat)
- Several colored spheres visible

**Verification:**
- [ ] Window displays correctly
- [ ] Blue throat ring visible
- [ ] At least 2-3 colored spheres visible
- [ ] No flickering or corruption

### Test 2: Camera Rotation
**Action:** Left-click and drag mouse horizontally

**Expected Behavior:**
- Camera orbits around the wormhole center
- Spheres change position relative to view
- Throat ring rotates in perspective
- Smooth motion (even if slow)

**Verification:**
- [ ] Camera responds to mouse drag
- [ ] View rotates around center point
- [ ] Objects remain properly rendered
- [ ] No crashes or freezes

### Test 3: Zoom Functionality
**Action:** Scroll mouse wheel

**Expected Behavior:**
- Scroll up: Camera moves closer (zoom in)
- Scroll down: Camera moves away (zoom out)
- View should adjust smoothly
- Objects grow/shrink in perspective

**Verification:**
- [ ] Zoom responds to scroll wheel
- [ ] Minimum distance limit works (can't go too close)
- [ ] Maximum distance limit works (can't go too far)
- [ ] No rendering artifacts

### Test 4: Two-Universe Visualization
**Action:** Rotate camera to different angles

**Expected Behavior:**
- From one side: See RGB colored spheres (Red, Green, Blue, Orange)
- Through throat: See distorted view of other universe
- From other side: See CMY colored spheres (Yellow, Magenta, Cyan, White)
- Background tint changes (reddish ↔ bluish)

**Verification:**
- [ ] Can see entrance universe spheres
- [ ] Can see exit universe spheres
- [ ] Objects visible through throat
- [ ] Background color changes

## Physics Verification Tests

### Test 5: Light Bending
**What to check:**
- Objects near throat should appear distorted
- Light paths visibly curve
- Throat region shows gravitational lensing effect

**Visual Indicators:**
- Spheres appear "stretched" near throat
- Multiple views of same object possible
- Blue glow indicates high curvature region

### Test 6: Geodesic Integration
**Technical Check:**

Monitor console output during rendering. Should see:
```
Frame: 10
Frame: 20
Frame: 30
...
```

**Verification:**
- [ ] Frame counter increments
- [ ] No error messages
- [ ] Rendering continues without crashes
- [ ] Memory usage stable (check Task Manager)

### Test 7: Universe Transition
**Advanced Test:**

**Setup:**
1. Position camera at entrance side (see RGB spheres)
2. Look directly at throat (blue ring)
3. Observe what's visible through it

**Expected:**
- Should see CMY-colored spheres through the throat
- These are from the exit universe
- They appear distorted but recognizable
- This proves ray tracing through the wormhole works

**Verification:**
- [ ] Can see through wormhole
- [ ] Different colored objects visible through throat
- [ ] Distortion is present
- [ ] No black voids (unless intentional escape rays)

## Performance Tests

### Test 8: Frame Rate
**Measure:** Count frames over 30 seconds

**Expected Performance:**
- **Fast CPU (modern i7/i9)**: 1-5 FPS
- **Medium CPU (i5/Ryzen 5)**: 0.5-2 FPS  
- **Slow CPU (older)**: 0.2-1 FPS

**Verification:**
- [ ] Consistent frame rate (not dropping)
- [ ] No memory leaks (check RAM usage)
- [ ] CPU usage high (expected - this is CPU-intensive)
- [ ] No overheating warnings

### Test 9: Stability
**Duration:** Run for 5 minutes

**Monitor:**
- Frame counter should reach ~150-300 frames
- Memory usage should be stable
- No crashes or hangs
- Responsive to input throughout

**Verification:**
- [ ] Runs for 5+ minutes without crash
- [ ] Memory doesn't keep growing
- [ ] Input remains responsive
- [ ] No graphical glitches over time

## Mathematical Verification

### Test 10: Throat Radius Accuracy
**Check:** The throat should be at minimum radius b₀ = 10^10 meters

**Visual Test:**
- Blue ring markers should form a circle
- Ring should be at origin (0, 0, 0)
- Size should be consistent

**Code Verification:**
```cpp
// In wormhole_sim.cpp
const double b0 = 1e10;  // Should be this value

// Check in debugger:
Ray ray = ...;
double r = ray.getR();
// r should always be >= b0
```

### Test 11: Geodesic Correctness
**Theoretical Property:** Rays should follow null geodesics

**Check:**
1. Energy E should be conserved
2. Angular momentum L should be conserved
3. Ray speed should equal c (light speed)

**To verify, add debugging output:**
```cpp
// In rk4Step function, add:
double E_check = ray.E;  // Should not change
double L_check = ray.L;  // Should not change
```

## Regression Tests

### Test 12: Scene Setup
**Verify object count:**
```cpp
// Should be:
// 4 spheres in universe -1
// 4 spheres in universe +1  
// 16 throat markers (universe 0)
// Total: 24 objects

assert(scene.size() == 24);
```

### Test 13: Coordinate Conversions
**Test roundtrip conversion:**
```cpp
vec3 original(1e10, 2e10, 3e10);
Ray ray(original, vec3(0, 0, 1));
ray.updateCartesian();

// Should get back to near-original
assert(length(ray.pos - original) < 1e6);  // Within 1000 km
```

## Common Issues & Solutions

### Issue: Black Screen
**Possible Causes:**
1. Camera inside an object
2. All rays escaping to infinity
3. OpenGL context failure

**Solutions:**
- Check camera.position is at (0, 0, 5e10)
- Verify scene objects were added
- Update graphics drivers

### Issue: Slow Performance
**Expected:** This is normal for CPU ray tracing

**Optimizations (if needed):**
```cpp
// Reduce resolution
const int WIDTH = 400;   // Instead of 800
const int HEIGHT = 300;  // Instead of 600

// Reduce integration steps
const int MAX_STEPS = 2000;  // Instead of 5000

// Increase step size (less accurate)
const double STEP_SIZE = 2e8;  // Instead of 1e8
```

### Issue: Objects Not Visible
**Debugging Steps:**
1. Check object positions in code
2. Verify camera is pointing at objects
3. Ensure universe tracking is correct
4. Add debug sphere at origin:
```cpp
scene.push_back(Sphere(vec3(0,0,0), 5e9, vec3(1,1,1), 0, true));
```

### Issue: Throat Not Glowing
**Check:**
- Throat markers were created (16 objects)
- They have `isEmissive = true`
- They're at universe = 0
- Camera is not too far away

## Validation Checklist

Before considering the simulation "working", verify:

- [x] Compiles without errors
- [ ] Window opens and displays
- [ ] Blue throat ring visible
- [ ] Colored spheres visible (both universes)
- [ ] Camera rotation works
- [ ] Zoom works
- [ ] Can see through wormhole
- [ ] Frame rate is reasonable (>0.1 FPS)
- [ ] Runs stable for 5+ minutes
- [ ] No memory leaks
- [ ] Console output is clean
- [ ] Objects are correctly positioned

## Benchmark Tests

### Minimal Scene (Performance Test)
```cpp
// Replace scene setup with just 2 spheres
scene.clear();
scene.push_back(Sphere(vec3(2e10, 0, -2e10), 5e9, vec3(1,0,0), -1));
scene.push_back(Sphere(vec3(2e10, 0, 2e10), 5e9, vec3(0,1,0), 1));
```

**Expected:** ~2x faster rendering

### Maximum Scene (Stress Test)
```cpp
// Add many objects
for (int i = 0; i < 100; i++) {
    float angle = (2*M_PI*i) / 100;
    vec3 pos(3e10*cos(angle), 3e10*sin(angle), -4e10);
    scene.push_back(Sphere(pos, 5e9, vec3(1,1,1), -1));
}
```

**Expected:** Slower but stable

## Final Verification

**System is fully working if:**
1. ✅ All visual tests pass
2. ✅ Physics behaves correctly
3. ✅ Performance is acceptable
4. ✅ Stable for extended use
5. ✅ No crashes or memory issues

**Next Steps:**
- Read `WORMHOLE_GUIDE.md` for usage tips
- Experiment with different viewing angles
- Try modifying parameters in code
- Consider GPU optimization for real-time performance

---

**Report Issues:** If tests fail, check dependencies and GPU drivers first.

**Success!** 🎉 You now have a working wormhole simulation!

