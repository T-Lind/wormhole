#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>
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
const int MOVIE_FPS = 24;

// Wormhole parameters
const float THROAT_RADIUS = 15.0f;  // Wormhole throat radius (b0)
const vec3 THROAT_CENTER = vec3(0, 0, 0);  // Throat location
const float BENDING_STRENGTH = 0.8f;  // How much light bends. Lower is less distortion.

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
    bool panning = false;
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

    void pan(float dx, float dy) {
        vec3 forward = normalize(target - position);
        vec3 right = normalize(cross(forward, up));
        vec3 localUp = normalize(cross(right, forward));
        float panSpeed = 0.1f * (radius / 100.0f);
        target -= right * dx * panSpeed;
        target += localUp * dy * panSpeed;
        updatePosition();
    }

    void zoom(float delta) {
        radius *= (1.0f - delta * 0.1f);
        radius = glm::clamp(radius, 20.0f, 500.0f);
        updatePosition();
    }
};

Camera camera;

// ============================================================================
// KEYBOARD INPUT
// ============================================================================

void processInput(GLFWwindow* window) {
    float cameraSpeed = 2.5f;
    vec3 forward = normalize(camera.target - camera.position);
    vec3 right = normalize(cross(forward, camera.up));
    vec3 localUp = cross(right, forward);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.position += forward * cameraSpeed;
        camera.target += forward * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.position -= forward * cameraSpeed;
        camera.target -= forward * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.position -= right * cameraSpeed;
        camera.target -= right * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.position += right * cameraSpeed;
        camera.target += right * cameraSpeed;
    }

    bool isShiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (isShiftPressed) {
            camera.position -= localUp * cameraSpeed;
            camera.target -= localUp * cameraSpeed;
        } else {
            camera.position += localUp * cameraSpeed;
            camera.target += localUp * cameraSpeed;
        }
    }
}


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
vector<vec4> stars; // x,y,z for direction, w for brightness

// ============================================================================
// STARFIELD
// ============================================================================
void generateStars(int count) {
    for (int i = 0; i < count; i++) {
        vec3 dir = normalize(vec3(
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f
        ));
        float brightness = (rand() / (float)RAND_MAX) * 0.5f + 0.5f;
        stars.push_back(vec4(dir, brightness));
    }
}

vec3 getStarfieldColor(const vec3& direction) {
    vec3 color = vec3(0.0);
    float fov_factor = 0.005; // a small value makes stars appear as points

    for(const auto& star : stars) {
        vec3 star_dir = vec3(star);
        float dist = acos(dot(direction, star_dir));
        
        // Using smoothstep to create a soft falloff for the star's glow
        float intensity = star.w * smoothstep(fov_factor, 0.0f, dist);
        
        if (intensity > 0.0) {
            color += vec3(intensity); // White stars, but could be colored
        }
    }
    return color;
}

// ============================================================================
// WORMHOLE THROAT (SPHERICAL)
// ============================================================================
struct WormholeThroat {
    vec3 center;
    float radius;  // Throat radius (b0)
    
    WormholeThroat(vec3 c, float r) : center(c), radius(r) {}
    
    // Check if ray intersects throat sphere
    bool intersect(const vec3& rayOrigin, const vec3& rayDir, float& t) const {
        vec3 oc = rayOrigin - center;
        float a = dot(rayDir, rayDir);
        float b = 2.0f * dot(oc, rayDir);
        float c_val = dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c_val;
        
        if (discriminant < 0) return false;
        
        float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
        if (t1 > 0.001f) {
            t = t1;
            return true;
        }
        
        float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
        if (t2 > 0.001f) {
            t = t2;
            return true;
        }
        
        return false;
    }
};

WormholeThroat throat(THROAT_CENTER, THROAT_RADIUS);

// ============================================================================
// RAY TRACING HELPER
// ============================================================================
struct HitInfo {
    bool hit = false;
    float distance = 1e10f;
    vec3 normal;
    vec3 color;
};

HitInfo traceScene(const vec3& origin, const vec3& direction, int universe) {
    HitInfo hitInfo;

    if (universe == 1) {
        // Check spheres
        for (const auto& sphere : spheres) {
            float t;
            vec3 normal;
            if (sphere.intersect(origin, direction, t, normal) && t < hitInfo.distance) {
                hitInfo.hit = true;
                hitInfo.distance = t;
                hitInfo.normal = normal;
                hitInfo.color = sphere.color;
            }
        }
    } else { // universe == 2
        // Check cubes
        for (const auto& cube : cubes) {
            float t;
            vec3 normal;
            if (cube.intersect(origin, direction, t, normal) && t < hitInfo.distance) {
                hitInfo.hit = true;
                hitInfo.distance = t;
                hitInfo.normal = normal;
                hitInfo.color = cube.color;
            }
        }
    }
    return hitInfo;
}


// ============================================================================
// RAY TRACING WITH GRAVITATIONAL LENSING
// ============================================================================
vec3 traceRay(const vec3& origin, const vec3& direction) {
    float t_throat;
    bool hitsThroat = throat.intersect(origin, direction, t_throat);

    HitInfo sceneHit = traceScene(origin, direction, currentUniverse);

    // Case 1: We hit a local object before the throat (or miss the throat entirely)
    if (sceneHit.hit && (!hitsThroat || sceneHit.distance < t_throat)) {
        vec3 viewDir = normalize(origin - (origin + direction * sceneHit.distance));
        vec3 lightDir = normalize(vec3(1, 1, 1));
        
        // Diffuse
        float diffuse = glm::max(0.0f, dot(sceneHit.normal, lightDir));
        
        // Specular
        vec3 reflectDir = reflect(-lightDir, sceneHit.normal);
        float spec = pow(glm::max(dot(viewDir, reflectDir), 0.0f), 32.0f);

        float ambient = 0.3f;
        return sceneHit.color * (ambient + diffuse * 0.7f) + vec3(0.5f) * spec;
    }

    // Case 2: We are looking at the wormhole
    if (hitsThroat) {
        vec3 P_in = origin + direction * t_throat;
        vec3 hit_normal = normalize(P_in - throat.center);

        // Fresnel effect for a glowing edge
        float fresnel = pow(1.0 - abs(dot(direction, hit_normal)), 3.0);

        // LENSING EFFECT: Refract the ray
        float ratio = 1.0 / 1.5; // "Refractive index" for lensing
        vec3 refracted_dir = refract(direction, hit_normal, ratio);
        
        // The new ray starts on the other side of the wormhole
        vec3 new_origin = P_in - hit_normal * (throat.radius * 2.0f);
        new_origin += refracted_dir * 0.1f;

        // Trace into the other universe
        int otherUniverse = (currentUniverse == 1) ? 2 : 1;
        HitInfo otherSceneHit = traceScene(new_origin, refracted_dir, otherUniverse);

        vec3 final_color;
        if (otherSceneHit.hit) {
            // We see an object through the wormhole
            vec3 viewDir = normalize(new_origin - (new_origin + refracted_dir * otherSceneHit.distance));
            vec3 lightDir = normalize(vec3(1, 1, 1));
            float diffuse = glm::max(0.0f, dot(otherSceneHit.normal, lightDir));
            vec3 reflectDir = reflect(-lightDir, otherSceneHit.normal);
            float spec = pow(glm::max(dot(viewDir, reflectDir), 0.0f), 32.0f);
            final_color = otherSceneHit.color * (0.3f + diffuse * 0.7f) + vec3(0.5f) * spec;
        } else {
            // We see the other universe's starfield
            final_color = getStarfieldColor(refracted_dir);
        }

        // Blend with Fresnel glow
        return mix(final_color, vec3(0.5, 0.8, 1.0), fresnel * 0.8f);
    }

    // Case 3: We missed everything, render local background
    if (currentUniverse == 1) {
        return getStarfieldColor(direction);
    } else {
        float t = 0.5f * (direction.y + 1.0f);
        return mix(vec3(0.02f, 0.01f, 0.01f), vec3(0.3f, 0.1f, 0.2f), t);
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
    
    void computePixels() {
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
                vec3 final_color(0.0f);
                const int samples = 2; // 2x2 Supersampling Anti-Aliasing
                for (int i = 0; i < samples; i++) {
                    for (int j = 0; j < samples; j++) {
                        float offX = (float(i) + 0.5f) / float(samples);
                        float offY = (float(j) + 0.5f) / float(samples);
                        float u = (2.0f * (x + offX) / WIDTH - 1.0f) * aspect * tanHalfFov;
                        float v = (1.0f - 2.0f * (y + offY) / HEIGHT) * tanHalfFov;
                        vec3 rayDir = normalize(u * right + v * up + forward);
                        final_color += traceRay(camera.position, rayDir);
                    }
                }
                final_color /= (float)(samples * samples);
                int idx = (y * WIDTH + x) * 3;
                pixels[idx + 0] = (unsigned char)(glm::clamp(final_color.r, 0.0f, 1.0f) * 255);
                pixels[idx + 1] = (unsigned char)(glm::clamp(final_color.g, 0.0f, 1.0f) * 255);
                pixels[idx + 2] = (unsigned char)(glm::clamp(final_color.b, 0.0f, 1.0f) * 255);
            }
        }
    }

    void drawPixels() {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    void render() {
        computePixels();
        drawPixels();
    }
};

// ============================================================================
// INPUT CALLBACKS
// ============================================================================
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_U && action == GLFW_PRESS) {
        currentUniverse = (currentUniverse == 1) ? 2 : 1;
        cout << "\nSwitched to Universe " << currentUniverse << "\n";
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            camera.dragging = true;
            camera.panning = (mods & GLFW_MOD_SHIFT);
            glfwGetCursorPos(window, &camera.lastX, &camera.lastY);
        } else {
            camera.dragging = false;
            camera.panning = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    if (camera.dragging) {
        float dx = x - camera.lastX;
        float dy = y - camera.lastY;
        if (camera.panning) {
            camera.pan(dx, dy);
        } else {
            camera.orbit(dx, dy);
        }
        camera.lastX = x;
        camera.lastY = y;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.zoom(yoffset);
}

// ============================================================================
// MOVIE MODE SUPPORT
// ============================================================================
struct Keyframe {
    float azimuthDeg;
    float elevationDeg;
    float radius;
    float durationSec;
};

static bool loadCameraPath(const std::string& pathFile, std::vector<Keyframe>& out) {
    std::ifstream in(pathFile);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        std::istringstream ss(line);
        Keyframe k{};
        if (!(ss >> k.azimuthDeg >> k.elevationDeg >> k.radius >> k.durationSec)) continue;
        out.push_back(k);
    }
    return !out.empty();
}

static void setCameraSpherical(float azimuthDeg, float elevationDeg, float radius) {
    camera.target = THROAT_CENTER;
    camera.azimuth = radians(azimuthDeg);
    camera.elevation = radians(elevationDeg);
    camera.radius = radius;
    camera.updatePosition();
}

static void writePPM(const std::string& filename, const std::vector<unsigned char>& buf, int w, int h) {
    std::ofstream out(filename, std::ios::binary);
    out << "P6\n" << w << " " << h << "\n255\n";
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(w*h*3));
}

static bool readPPM(const std::string& filename, std::vector<unsigned char>& buf, int& w, int& h) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;
    std::string magic; in >> magic; if (magic != "P6") return false;
    char c = in.peek();
    while (c == '#') { std::string comment; std::getline(in, comment); c = in.peek(); }
    in >> w >> h; int maxv; in >> maxv; in.get();
    if (maxv != 255) return false;
    buf.resize(w*h*3);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(w*h*3));
    return true;
}

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char** argv) {
    bool interactive = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--interactive" || a == "-i") interactive = true;
    }
    Engine engine;
    
    // Set up input callbacks
    glfwSetKeyCallback(engine.window, keyCallback);
    glfwSetMouseButtonCallback(engine.window, mouseButtonCallback);
    glfwSetCursorPosCallback(engine.window, cursorPosCallback);
    glfwSetScrollCallback(engine.window, scrollCallback);
    
    // Create two distinct UNIVERSES
    cout << "\n=== WORMHOLE PORTAL SIMULATION ===\n\n";
    
    // UNIVERSE 1: SPHERES with RGB colors (you start here!)
    spheres.push_back(Sphere(vec3(-80, 40, 0), 10, vec3(1.0f, 0.2f, 0.2f)));    // Red
    spheres.push_back(Sphere(vec3(-80, -40, 0), 10, vec3(0.2f, 1.0f, 0.2f)));   // Green
    spheres.push_back(Sphere(vec3(-100, 0, 50), 10, vec3(0.2f, 0.2f, 1.0f)));    // Blue
    spheres.push_back(Sphere(vec3(-120, 0, 0), 12, vec3(1.0f, 0.5f, 0.0f)));     // Orange
    
    // UNIVERSE 2: CUBES with CMY colors (visible ONLY through portal!)
    cubes.push_back(Cube(vec3(80, 40, 0), 18, vec3(1.0f, 1.0f, 0.2f)));     // Yellow
    cubes.push_back(Cube(vec3(80, -40, 0), 18, vec3(1.0f, 0.2f, 1.0f)));    // Magenta
    cubes.push_back(Cube(vec3(100, 0, 50), 18, vec3(0.2f, 1.0f, 1.0f)));     // Cyan
    cubes.push_back(Cube(vec3(120, 0, 0), 22, vec3(1.0f, 1.0f, 1.0f)));      // White
    
    currentUniverse = 1;  // Start in Universe 1 (spheres)
    
    generateStars(1000); // Generate stars for universe 2

    cout << "Universe 1 (Current): " << spheres.size() << " SPHERES (RGB colors)\n";
    cout << "Universe 2 (Through throat): " << cubes.size() << " CUBES (CMY colors)\n";
    cout << "\nWormhole Throat: " << THROAT_RADIUS << " unit radius SPHERE at origin\n";
    cout << "Light Bending: ENABLED (Refraction-based Lensing)\n";
    cout << "Visuals: ADDED Starfield, Fresnel Glow, Specular Highlights\n";
    cout << "Camera at: (" << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")\n";
    cout << "Looking at: (" << camera.target.x << ", " << camera.target.y << ", " << camera.target.z << ")\n\n";
    
    cout << "Controls:\n";
    cout << "  Left Mouse Drag: Orbit camera\n";
    cout << "  Shift + Left Drag: Pan camera\n";
    cout << "  Mouse Scroll: Zoom in/out\n";
    cout << "  W/A/S/D: Move camera\n";
    cout << "  Space/Shift+Space: Move up/down\n";
    cout << "  U: Switch universe\n";
    cout << "  ESC: Exit\n\n";
    
    cout << "What you'll see:\n";
    cout << "  - SPHERES with specular highlights in your universe\n";
    cout << "  - A glowing, glass-like SPHERICAL wormhole throat\n";
    cout << "  - A STARFIELD background visible only through the wormhole\n";
    cout << "  - SMOOTH, high-quality rendering (due to anti-aliasing)\n";
    cout << "  - Objects will appear DISTORTED through the throat\n\n";
    
    if (interactive) {
        cout << "Rendering (interactive)...\n";
        int frameCount = 0;
        while (!glfwWindowShouldClose(engine.window)) {
            processInput(engine.window);
            engine.render();
            if (++frameCount % 10 == 0) {
                cout << "Frame " << frameCount << "\r" << flush;
            }
        }
        cout << "\n\nSimulation ended. Total frames: " << frameCount << "\n";
        glfwTerminate();
        return 0;
    }

    // Movie mode (default): precompute frames, then play back
    cout << "Movie mode: precomputing frames...\n";
    std::vector<Keyframe> keys;
    std::string pathFile = "camera_path.txt";
    if (!loadCameraPath(pathFile, keys)) {
        keys = { {0,60,200,2},{90,55,160,2},{180,50,120,2},{270,45,80,2},{360,40,50,2},{540,35,30,2} };
        cout << "Warning: camera_path.txt not found. Using built-in path.\n";
    }

    std::filesystem::create_directories("exports");
    std::vector<std::string> frameFiles;

    int frameIndex = 0;
    for (size_t k = 0; k + 1 < keys.size(); ++k) {
        const Keyframe& a = keys[k];
        const Keyframe& b = keys[k+1];
        int frames = std::max(1, int(std::round(a.durationSec * MOVIE_FPS)));
        for (int f = 0; f < frames; ++f) {
            float t = (frames == 1) ? 1.0f : (float(f) / float(frames));
            float az = glm::mix(a.azimuthDeg,   b.azimuthDeg,   t);
            float el = glm::mix(a.elevationDeg, b.elevationDeg, t);
            float rr = glm::mix(a.radius,       b.radius,       t);
            setCameraSpherical(az, el, rr);
            engine.computePixels();
            std::ostringstream name;
            name << "exports/frame_" << std::setw(5) << std::setfill('0') << frameIndex++ << ".ppm";
            std::string file = name.str();
            writePPM(file, engine.pixels, WIDTH, HEIGHT);
            frameFiles.push_back(file);
            if (frameIndex % 10 == 0) cout << "Saved " << frameIndex << " frames\r" << flush;
        }
    }
    cout << "\nPrecompute complete: " << frameIndex << " frames written to exports/.\n";

    cout << "Playing back... (" << MOVIE_FPS << " FPS)\n";
    glfwSetWindowTitle(engine.window, "Wormhole Simulation - Playback");
    for (const auto& file : frameFiles) {
        std::vector<unsigned char> buf;
        int w=0,h=0;
        if (!readPPM(file, buf, w, h)) continue;
        if (w != WIDTH || h != HEIGHT) continue;
        engine.pixels = std::move(buf);
        engine.drawPixels();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / MOVIE_FPS));
        if (glfwWindowShouldClose(engine.window)) break;
    }

    glfwTerminate();
    return 0;
}
