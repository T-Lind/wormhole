#include <windows.h>

// Force dedicated GPU on laptops
extern "C" {
  __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

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
struct alignas(16) Camera {
    vec3 position;
    float _pad1; // Padding for std140 alignment
    vec3 target;
    float _pad2; // Padding
    vec3 up;
    float _pad3; // Padding
    float fov;
    float azimuth, elevation, radius;
    bool dragging = false;
    bool panning = false;
    double lastX = 0, lastY = 0;

    Camera() : position(0, 0, 80.0f), target(0, 0, 0), up(0, 1, 0), 
               fov(60.0f), azimuth(0), elevation(M_PI/2), radius(80.0f) {}

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
struct alignas(16) Sphere {
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

struct alignas(16) Cube {
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

struct alignas(16) Star {
    vec4 data; // .xyz = direction, .w = brightness
};

vector<Sphere> spheres;
vector<Cube> cubes;
vector<Star> stars;

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
        stars.push_back({vec4(dir, brightness)});
    }
}

vec3 getStarfieldColor(const vec3& direction) {
    vec3 color = vec3(0.0);
    float fov_factor = 0.005; // a small value makes stars appear as points

    for(const auto& star : stars) {
        vec3 star_dir = vec3(star.data);
        float dist = acos(dot(direction, star_dir));
        
        // Using smoothstep to create a soft falloff for the star's glow
        float intensity = star.data.w * smoothstep(fov_factor, 0.0f, dist);
        
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

    // For compute shader
    GLuint computeShaderProgram;
    GLuint cameraUBO;
    GLuint spheresSSBO;
    GLuint cubesSSBO;
    GLuint starsSSBO;
    
    Engine() {
        pixels.resize(WIDTH * HEIGHT * 3);
        initGLFW();
        initShaders();
        initQuad();
        initCompute(); // Initializes shaders and UBOs, but not SSBOs yet
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
    
    // Helper to read shader file
    std::string readShaderFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "ERROR: Could not open shader file: " << path << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void initCompute() {
        std::string computeShaderSource = readShaderFromFile("wormhole.comp");
        const char* css_c = computeShaderSource.c_str();

        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &css_c, NULL);
        glCompileShader(computeShader);

        // Check for compilation errors
        int success;
        char infoLog[1024];
        glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(computeShader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        computeShaderProgram = glCreateProgram();
        glAttachShader(computeShaderProgram, computeShader);
        glLinkProgram(computeShaderProgram);

        // Check for linking errors
        glGetProgramiv(computeShaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(computeShaderProgram, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPUTE::LINKING_FAILED\n" << infoLog << std::endl;
        }
        glDeleteShader(computeShader);

        // Create buffers
        glGenBuffers(1, &cameraUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Camera), NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);

        // Create SSBOs but don't load data yet
        glGenBuffers(1, &spheresSSBO);
        glGenBuffers(1, &cubesSSBO);
        glGenBuffers(1, &starsSSBO);
    }

    void uploadSceneData() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, spheresSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(Sphere), spheres.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spheresSSBO);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cubesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, cubes.size() * sizeof(Cube), cubes.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cubesSSBO);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, starsSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, stars.size() * sizeof(Star), stars.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, starsSSBO);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    }
    
    void computePixels() {
        glUseProgram(computeShaderProgram);

        // Update Camera UBO
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Camera), &camera);
        
        // Update uniforms
        glUniform1i(glGetUniformLocation(computeShaderProgram, "currentUniverse"), currentUniverse);
        glUniform1i(glGetUniformLocation(computeShaderProgram, "numSpheres"), spheres.size());
        glUniform1i(glGetUniformLocation(computeShaderProgram, "numCubes"), cubes.size());
        glUniform1i(glGetUniformLocation(computeShaderProgram, "numStars"), stars.size());
        
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(WIDTH / 8, HEIGHT / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void drawPixels() {
        // No need to upload pixels from CPU anymore
        // Just draw the texture that the compute shader wrote to
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture); // Make sure texture unit 0 is active
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
        // No need to re-upload data as shader handles visibility via uniform
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
    float timeSec;
    // Position (spherical)
    float posAzimuthDeg;
    float posElevationDeg;
    float posRadius;
    // Target (cartesian)
    vec3 target;
};

static bool loadCameraPath(const std::string& pathFile, std::vector<Keyframe>& out) {
    std::ifstream in(pathFile);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        Keyframe k{};
        if (!(ss >> k.timeSec >> k.posAzimuthDeg >> k.posElevationDeg >> k.posRadius >> k.target.x >> k.target.y >> k.target.z)) continue;
        out.push_back(k);
    }
    return !out.empty();
}

static void setCamera(const vec3& pos, const vec3& target) {
    camera.position = pos;
    camera.target = target;
    // Recalculate spherical coordinates from new pos/target for orbiting controls (if needed)
    vec3 dir = target - pos;
    camera.radius = length(dir);
    camera.azimuth = atan2(dir.z, dir.x);
    camera.elevation = acos(dir.y / camera.radius);
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
    
    generateStars(1000); // Generate stars BEFORE uploading data
    engine.uploadSceneData(); // NOW we upload the data, after it's created

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
        cout << "Rendering (interactive)... Use -i command line flag to switch modes.\n";
        int frameCount = 0;
        double lastTime = glfwGetTime();

        while (!glfwWindowShouldClose(engine.window)) {
            processInput(engine.window);
            engine.render();

            // FPS Counter
            frameCount++;
            double currentTime = glfwGetTime();
            double elapsedTime = currentTime - lastTime;
            if (elapsedTime >= 1.0) {
                double fps = double(frameCount) / elapsedTime;
                std::stringstream ss;
                ss << "Wormhole Simulation | " << spheres.size() << " Spheres, " << cubes.size() << " Cubes | FPS: " << std::fixed << std::setprecision(1) << fps;
                glfwSetWindowTitle(engine.window, ss.str().c_str());
                
                frameCount = 0;
                lastTime = currentTime;
            }
        }
        cout << "\n\nSimulation ended.\n";
        glfwTerminate();
        return 0;
    }

    // Movie mode (default): precompute frames, then play back
    cout << "Movie mode: precomputing frames...\n";
    std::vector<Keyframe> keys;
    std::string pathFile = "camera_path.txt";
    if (!loadCameraPath(pathFile, keys)) {
        cout << "Error: camera_path.txt not found or invalid. Using built-in path.\n";
        keys = {
            {0.0f, 0, 80, 250, {0,0,0}},
            {2.0f, 90, 70, 200, {10,10,0}},
            {4.0f, 180, 60, 150, {0,0,0}},
            {6.0f, 270, 50, 100, {-10,-10,0}},
            {8.0f, 360, 40, 70, {0,0,0}},
            {10.0f, 540, 35, 45, {5,0,5}},
            {12.0f, 720, 30, 25, {0,0,0}}
        };
    }

    // Create timestamped directory for exports
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "exports/run_" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
    std::string exportDir = ss.str();
    std::filesystem::create_directories(exportDir);

    std::vector<std::string> frameFiles;
    float totalDuration = keys.back().timeSec;
    int totalFrames = static_cast<int>(totalDuration * MOVIE_FPS);
    
    cout << "Rendering " << totalFrames << " frames for a " << totalDuration << "s video...\n";

    // Build frame list via linear interpolation between keyframes
    size_t keyframe_idx = 0;
    for (int i = 0; i < totalFrames; ++i) {
        float currentTime = static_cast<float>(i) / MOVIE_FPS;

        // Find the two keyframes to interpolate between
        while (keyframe_idx + 1 < keys.size() && keys[keyframe_idx + 1].timeSec < currentTime) {
            keyframe_idx++;
        }

        const Keyframe& a = keys[keyframe_idx];
        const Keyframe& b = keys[keyframe_idx + 1 < keys.size() ? keyframe_idx + 1 : keyframe_idx];

        float t = 0.0f;
        if (b.timeSec > a.timeSec) {
            t = (currentTime - a.timeSec) / (b.timeSec - a.timeSec);
        }

        float az = glm::mix(a.posAzimuthDeg, b.posAzimuthDeg, t);
        float el = glm::mix(a.posElevationDeg, b.posElevationDeg, t);
        float rr = glm::mix(a.posRadius, b.posRadius, t);
        vec3 interpolated_target = glm::mix(a.target, b.target, t);

        vec3 pos;
        pos.x = interpolated_target.x + rr * sin(radians(el)) * cos(radians(az));
        pos.y = interpolated_target.y + rr * cos(radians(el));
        pos.z = interpolated_target.z + rr * sin(radians(el)) * sin(radians(az));
        
        setCamera(pos, interpolated_target);

        engine.computePixels();
        std::ostringstream name;
        name << exportDir << "/frame_" << std::setw(5) << std::setfill('0') << i << ".ppm";
        std::string file = name.str();

        // We need to read the pixels back from the GPU texture to save them
        std::vector<float> gpu_pixels(WIDTH * HEIGHT * 4);
        glBindTexture(GL_TEXTURE_2D, engine.texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, gpu_pixels.data());

        // Convert float RGBA to uchar RGB for PPM
        std::vector<unsigned char> ppm_pixels(WIDTH * HEIGHT * 3);
        for(int y = 0; y < HEIGHT; ++y) {
            for(int x = 0; x < WIDTH; ++x) {
                int flipped_y = HEIGHT - 1 - y; // PPM is top-down
                int gpu_idx = (y * WIDTH + x) * 4;
                int ppm_idx = (flipped_y * WIDTH + x) * 3;
                ppm_pixels[ppm_idx + 0] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 0], 0.0f, 1.0f) * 255);
                ppm_pixels[ppm_idx + 1] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 1], 0.0f, 1.0f) * 255);
                ppm_pixels[ppm_idx + 2] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 2], 0.0f, 1.0f) * 255);
            }
        }
        writePPM(file, ppm_pixels, WIDTH, HEIGHT);
        
        frameFiles.push_back(file);
        cout << "Saved frame " << (i + 1) << "/" << totalFrames << "\r" << flush;
    }
    cout << "\nPrecompute complete: " << totalFrames << " frames written to " << exportDir << "/.\n";

    // Automatically run ffmpeg
    cout << "Running FFmpeg to create video...\n";
    std::string videoFile = ss.str() + ".mp4";
    std::string ffmpeg_cmd = "ffmpeg -r " + std::to_string(MOVIE_FPS) 
                           + " -i " + exportDir + "/frame_%05d.ppm"
                           + " -c:v libx264 -pix_fmt yuv420p " + videoFile;

    int ffmpeg_ret = system(ffmpeg_cmd.c_str());
    if (ffmpeg_ret == 0) {
        cout << "Successfully created video: " << videoFile << "\n";
    } else {
        cout << "Error: FFmpeg command failed. Could not create video.\n";
        cout << "You can try running the command manually:\n" << ffmpeg_cmd << "\n";
    }

    // Playback
    cout << "Playing back... (" << MOVIE_FPS << " FPS)\n";
    glfwSetWindowTitle(engine.window, "Wormhole Simulation - Playback");
    for (const auto& file : frameFiles) {
        std::vector<unsigned char> buf;
        int w=0,h=0;
        if (!readPPM(file, buf, w, h)) continue;
        if (w != WIDTH || h != HEIGHT) continue;
        
        // Convert RGB uchar back to RGBA float for texture
        std::vector<float> gpu_pixels(WIDTH * HEIGHT * 4);
        for(int y = 0; y < HEIGHT; ++y) {
            for(int x = 0; x < WIDTH; ++x) {
                 int flipped_y = HEIGHT - 1 - y;
                 int ppm_idx = (y * WIDTH + x) * 3;
                 int gpu_idx = (flipped_y * WIDTH + x) * 4;
                 gpu_pixels[gpu_idx + 0] = buf[ppm_idx + 0] / 255.0f;
                 gpu_pixels[gpu_idx + 1] = buf[ppm_idx + 1] / 255.0f;
                 gpu_pixels[gpu_idx + 2] = buf[ppm_idx + 2] / 255.0f;
                 gpu_pixels[gpu_idx + 3] = 1.0f;
            }
        }
        
        glBindTexture(GL_TEXTURE_2D, engine.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGBA, GL_FLOAT, gpu_pixels.data());
        engine.drawPixels();

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / MOVIE_FPS));
        if (glfwWindowShouldClose(engine.window)) break;
    }

    glfwTerminate();
    return 0;
}
