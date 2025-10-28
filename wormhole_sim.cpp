#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace glm;
using namespace std;

// ============================================================================
// CONSTANTS
// ============================================================================
const int WIDTH = 800;
const int HEIGHT = 600;

// Wormhole portal parameters
const float PORTAL_RADIUS = 15.0f;  // Portal size
const vec3 PORTAL_CENTER = vec3(0, 0, 0);  // Portal location

// Which universe is the camera in? (1 = spheres, 2 = cubes)
int currentUniverse = 1;

// ============================================================================
// CAMERA
// ============================================================================
struct Camera {
    vec3 position;
    vec3 target;
    vec3 up;
    float fov;
    float azimuth, elevation, radius;
    bool dragging = false;
    double lastX = 0, lastY = 0;

    Camera() : position(0, 0, 100.0f), target(0, 0, 0), up(0, 1, 0), 
               fov(60.0f), azimuth(0), elevation(M_PI/2), radius(100.0f) {}

    void updatePosition() {
        position.x = target.x + radius * sin(elevation) * cos(azimuth);
        position.y = target.y + radius * cos(elevation);
        position.z = target.z + radius * sin(elevation) * sin(azimuth);
    }

    void orbit(float dx, float dy) {
        azimuth -= dx * 0.01f;
        elevation += dy * 0.01f;
        elevation = glm::clamp(elevation, 0.1f, float(M_PI) - 0.1f);
        updatePosition();
    }

    void zoom(float delta) {
        radius *= (1.0f - delta * 0.1f);
        radius = glm::clamp(radius, 10.0f, 500.0f);
        updatePosition();
    }
};

Camera camera;

// ============================================================================
// SCENE OBJECTS
// ============================================================================
struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
    
    Sphere(vec3 c, float r, vec3 col) 
        : center(c), radius(r), color(col) {}
    
    bool intersect(const vec3& rayOrigin, const vec3& rayDir, float& t, vec3& normal) const {
        vec3 oc = rayOrigin - center;
        float a = dot(rayDir, rayDir);
        float b = 2.0f * dot(oc, rayDir);
        float c = dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant < 0) return false;
        
        float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
        if (t1 > 0.001f) {
            t = t1;
            vec3 hitPoint = rayOrigin + rayDir * t;
            normal = normalize(hitPoint - center);
            return true;
        }
        
        float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
        if (t2 > 0.001f) {
            t = t2;
            vec3 hitPoint = rayOrigin + rayDir * t;
            normal = normalize(hitPoint - center);
            return true;
        }
        
        return false;
    }
};

struct Cube {
    vec3 center;
    float size;
    vec3 color;
    
    Cube(vec3 c, float s, vec3 col) 
        : center(c), size(s), color(col) {}
    
    bool intersect(const vec3& rayOrigin, const vec3& rayDir, float& t, vec3& normal) const {
        // Axis-aligned bounding box intersection
        vec3 minBound = center - vec3(size/2);
        vec3 maxBound = center + vec3(size/2);
        
        vec3 invDir = vec3(1.0f) / rayDir;
        vec3 t0 = (minBound - rayOrigin) * invDir;
        vec3 t1 = (maxBound - rayOrigin) * invDir;
        
        vec3 tmin = glm::min(t0, t1);
        vec3 tmax = glm::max(t0, t1);
        
        float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
        
        if (tNear > tFar || tFar < 0.001f) return false;
        
        t = (tNear > 0.001f) ? tNear : tFar;
        
        // Calculate normal based on which face was hit
        vec3 hitPoint = rayOrigin + rayDir * t;
        vec3 d = hitPoint - center;
        vec3 absD = glm::abs(d);
        float halfSize = size / 2.0f;
        
        if (absD.x > absD.y && absD.x > absD.z) {
            normal = vec3(glm::sign(d.x), 0, 0);
        } else if (absD.y > absD.z) {
            normal = vec3(0, glm::sign(d.y), 0);
        } else {
            normal = vec3(0, 0, glm::sign(d.z));
        }
        
        return true;
    }
};

vector<Sphere> spheres;
vector<Cube> cubes;

// ============================================================================
// PORTAL / WORMHOLE
// ============================================================================
struct Portal {
    vec3 center;
    vec3 normal;  // Portal facing direction
    float radius;
    
    Portal(vec3 c, vec3 n, float r) : center(c), normal(normalize(n)), radius(r) {}
    
    // Check if ray intersects portal disk
    bool intersect(const vec3& rayOrigin, const vec3& rayDir, float& t) const {
        // Plane intersection
        float denom = dot(normal, rayDir);
        if (abs(denom) < 0.0001f) return false;  // Parallel to plane
        
        t = dot(center - rayOrigin, normal) / denom;
        if (t < 0.001f) return false;  // Behind ray
        
        // Check if hit point is within disk radius
        vec3 hitPoint = rayOrigin + rayDir * t;
        float dist = length(hitPoint - center);
        
        return dist <= radius;
    }
};

Portal portal(PORTAL_CENTER, vec3(0, 0, 1), PORTAL_RADIUS);

// ============================================================================
// RAY TRACING WITH PORTAL CONNECTION
// ============================================================================
vec3 traceRay(const vec3& origin, const vec3& direction) {
    // Check if ray hits the portal
    float portal_t;
    bool hitPortal = portal.intersect(origin, direction, portal_t);
    
    float closest_t = 1e10f;
    vec3 hitNormal;
    vec3 hitColor;
    bool hitSomething = false;
    int hitUniverse = currentUniverse;  // Which universe did we hit something in?
    
    // Only check objects in CURRENT universe (unless we go through portal)
    if (currentUniverse == 1) {
        // We're in Universe 1 (spheres) - only check spheres normally
        for (const auto& sphere : spheres) {
            float t;
            vec3 normal;
            if (sphere.intersect(origin, direction, t, normal) && t < closest_t) {
                closest_t = t;
                hitNormal = normal;
                hitColor = sphere.color;
                hitSomething = true;
                hitUniverse = 1;
            }
        }
    } else {
        // We're in Universe 2 (cubes) - only check cubes normally
        for (const auto& cube : cubes) {
            float t;
            vec3 normal;
            if (cube.intersect(origin, direction, t, normal) && t < closest_t) {
                closest_t = t;
                hitNormal = normal;
                hitColor = cube.color;
                hitSomething = true;
                hitUniverse = 2;
            }
        }
    }
    
    // If we hit portal BEFORE hitting any object, look through to other universe!
    if (hitPortal && portal_t < closest_t) {
        // Ray goes through portal! Now check OTHER universe
        vec3 portalHitPoint = origin + direction * portal_t;
        
        // Continue ray from portal into other universe
        // For now, just continue straight (no bending yet)
        vec3 newOrigin = portalHitPoint + direction * 0.1f;  // Small offset
        
        float otherUniverse_t = 1e10f;
        bool hitOtherSide = false;
        
        if (currentUniverse == 1) {
            // Looking through from Universe 1 → see Universe 2 (cubes)
            for (const auto& cube : cubes) {
                float t;
                vec3 normal;
                if (cube.intersect(newOrigin, direction, t, normal) && t < otherUniverse_t) {
                    otherUniverse_t = t;
                    hitNormal = normal;
                    hitColor = cube.color;
                    hitOtherSide = true;
                }
            }
        } else {
            // Looking through from Universe 2 → see Universe 1 (spheres)
            for (const auto& sphere : spheres) {
                float t;
                vec3 normal;
                if (sphere.intersect(newOrigin, direction, t, normal) && t < otherUniverse_t) {
                    otherUniverse_t = t;
                    hitNormal = normal;
                    hitColor = sphere.color;
                    hitOtherSide = true;
                }
            }
        }
        
        if (hitOtherSide) {
            // Shade the object from the other universe
            vec3 lightDir = normalize(vec3(1, 1, 1));
            float diffuse = glm::max(0.0f, dot(hitNormal, lightDir));
            float ambient = 0.3f;
            return hitColor * (ambient + (1.0f - ambient) * diffuse);
        } else {
            // Portal shows other universe's background
            float t = 0.5f * (direction.y + 1.0f);
            if (currentUniverse == 1) {
                // Looking into Universe 2 - greenish tint
                return mix(vec3(0.05f, 0.15f, 0.05f), vec3(0.2f, 0.8f, 0.4f), t);
            } else {
                // Looking into Universe 1 - reddish tint
                return mix(vec3(0.15f, 0.05f, 0.05f), vec3(0.8f, 0.4f, 0.2f), t);
            }
        }
    }
    
    // Visualize portal rim (glowing edge)
    if (hitPortal && portal_t < closest_t * 1.1f) {
        vec3 portalHitPoint = origin + direction * portal_t;
        float distFromCenter = length(portalHitPoint - PORTAL_CENTER);
        if (distFromCenter > PORTAL_RADIUS * 0.9f) {
            // Near edge - glow
            float edge = (distFromCenter - PORTAL_RADIUS * 0.9f) / (PORTAL_RADIUS * 0.1f);
            return mix(vec3(0.3f, 0.6f, 1.0f), vec3(0, 0, 0), edge);
        }
    }
    
    // Regular hit in current universe
    if (hitSomething) {
        vec3 lightDir = normalize(vec3(1, 1, 1));
        float diffuse = glm::max(0.0f, dot(hitNormal, lightDir));
        float ambient = 0.3f;
        return hitColor * (ambient + (1.0f - ambient) * diffuse);
    }
    
    // Background for current universe
    float t = 0.5f * (direction.y + 1.0f);
    if (currentUniverse == 1) {
        // Universe 1 background - bluish
        return mix(vec3(0.1f, 0.1f, 0.15f), vec3(0.4f, 0.5f, 0.8f), t);
    } else {
        // Universe 2 background - purplish
        return mix(vec3(0.15f, 0.1f, 0.15f), vec3(0.6f, 0.4f, 0.8f), t);
    }
}

// ============================================================================
// RENDERING ENGINE
// ============================================================================
struct Engine {
    GLFWwindow* window;
    GLuint quadVAO, quadVBO;
    GLuint texture;
    GLuint shaderProgram;
    vector<unsigned char> pixels;
    
    Engine() {
        pixels.resize(WIDTH * HEIGHT * 3);
        initGLFW();
        initShaders();
        initQuad();
    }
    
    void initGLFW() {
        if (!glfwInit()) {
            cerr << "Failed to initialize GLFW\n";
            exit(EXIT_FAILURE);
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        window = glfwCreateWindow(WIDTH, HEIGHT, "Wormhole Simulation (Simple Scene)", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        
        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        
        if (glewInit() != GLEW_OK) {
            cerr << "Failed to initialize GLEW\n";
            exit(EXIT_FAILURE);
        }
        
        cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    }
    
    void initShaders() {
        const char* vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            })";
        
        const char* fragSrc = R"(
            #version 330 core
            in vec2 TexCoord;
            out vec4 FragColor;
            uniform sampler2D screenTexture;
            void main() {
                FragColor = texture(screenTexture, TexCoord);
            })";
        
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vertSrc, nullptr);
        glCompileShader(vert);
        
        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &fragSrc, nullptr);
        glCompileShader(frag);
        
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vert);
        glAttachShader(shaderProgram, frag);
        glLinkProgram(shaderProgram);
        
        glDeleteShader(vert);
        glDeleteShader(frag);
    }
    
    void initQuad() {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    void render() {
        // Build camera basis
        vec3 forward = normalize(camera.target - camera.position);
        vec3 right = normalize(cross(forward, camera.up));
        vec3 up = cross(right, forward);
        
        float aspect = float(WIDTH) / float(HEIGHT);
        float tanHalfFov = tan(radians(camera.fov) * 0.5f);
        
        // Ray trace each pixel (parallelized for speed!)
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                // Calculate ray direction
                float u = (2.0f * (x + 0.5f) / WIDTH - 1.0f) * aspect * tanHalfFov;
                float v = (1.0f - 2.0f * (y + 0.5f) / HEIGHT) * tanHalfFov;
                
                vec3 rayDir = normalize(u * right + v * up + forward);
                vec3 color = traceRay(camera.position, rayDir);
                
                // Write to pixel buffer
                int idx = (y * WIDTH + x) * 3;
                pixels[idx + 0] = (unsigned char)(glm::clamp(color.r, 0.0f, 1.0f) * 255);
                pixels[idx + 1] = (unsigned char)(glm::clamp(color.g, 0.0f, 1.0f) * 255);
                pixels[idx + 2] = (unsigned char)(glm::clamp(color.b, 0.0f, 1.0f) * 255);
            }
        }
        
        // Upload to GPU
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        
        // Draw fullscreen quad
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
};

// ============================================================================
// INPUT CALLBACKS
// ============================================================================
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            camera.dragging = true;
            glfwGetCursorPos(window, &camera.lastX, &camera.lastY);
        } else {
            camera.dragging = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    if (camera.dragging) {
        float dx = x - camera.lastX;
        float dy = y - camera.lastY;
        camera.orbit(dx, dy);
        camera.lastX = x;
        camera.lastY = y;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.zoom(yoffset);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    Engine engine;
    
    // Set up input callbacks
    glfwSetMouseButtonCallback(engine.window, mouseButtonCallback);
    glfwSetCursorPosCallback(engine.window, cursorPosCallback);
    glfwSetScrollCallback(engine.window, scrollCallback);
    
    // Create two distinct UNIVERSES
    cout << "\n=== WORMHOLE PORTAL SIMULATION ===\n\n";
    
    // UNIVERSE 1: SPHERES with RGB colors (you start here!)
    spheres.push_back(Sphere(vec3(-40, 20, 0), 10, vec3(1.0f, 0.2f, 0.2f)));    // Red
    spheres.push_back(Sphere(vec3(-40, -20, 0), 10, vec3(0.2f, 1.0f, 0.2f)));   // Green  
    spheres.push_back(Sphere(vec3(-40, 0, 25), 10, vec3(0.2f, 0.2f, 1.0f)));    // Blue
    spheres.push_back(Sphere(vec3(-60, 0, 0), 12, vec3(1.0f, 0.5f, 0.0f)));     // Orange
    
    // UNIVERSE 2: CUBES with CMY colors (visible ONLY through portal!)
    cubes.push_back(Cube(vec3(40, 20, 0), 18, vec3(1.0f, 1.0f, 0.2f)));     // Yellow
    cubes.push_back(Cube(vec3(40, -20, 0), 18, vec3(1.0f, 0.2f, 1.0f)));    // Magenta
    cubes.push_back(Cube(vec3(40, 0, 25), 18, vec3(0.2f, 1.0f, 1.0f)));     // Cyan
    cubes.push_back(Cube(vec3(60, 0, 0), 22, vec3(1.0f, 1.0f, 1.0f)));      // White
    
    currentUniverse = 1;  // Start in Universe 1 (spheres)
    
    cout << "Universe 1 (Current): " << spheres.size() << " SPHERES (RGB colors)\n";
    cout << "Universe 2 (Through portal): " << cubes.size() << " CUBES (CMY colors)\n";
    cout << "\nPortal: " << PORTAL_RADIUS << " unit radius disk at origin\n";
    cout << "Camera at: (" << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")\n";
    cout << "Looking at: (" << camera.target.x << ", " << camera.target.y << ", " << camera.target.z << ")\n\n";
    
    cout << "Controls:\n";
    cout << "  Left Mouse Drag: Orbit camera\n";
    cout << "  Mouse Scroll: Zoom in/out\n";
    cout << "  ESC: Exit\n\n";
    
    cout << "What you'll see:\n";
    cout << "  - Colorful SPHERES in your universe (RGB colors)\n";
    cout << "  - Blue glowing PORTAL disk at center\n";
    cout << "  - Look THROUGH portal to see CUBES from other universe!\n";
    cout << "  - Cubes are ONLY visible through the portal\n\n";
    
    cout << "Rendering...\n";
    
    int frameCount = 0;
    while (!glfwWindowShouldClose(engine.window)) {
        engine.render();
        
        // Print progress every 10 frames
        if (++frameCount % 10 == 0) {
            cout << "Frame " << frameCount << "\r" << flush;
        }
    }
    
    cout << "\n\nSimulation ended. Total frames: " << frameCount << "\n";
    glfwTerminate();
    return 0;
}
