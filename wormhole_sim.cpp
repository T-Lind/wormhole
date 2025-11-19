#ifdef _WIN32
#include <windows.h>

// Force dedicated GPU on laptops
extern "C" {
  __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace glm;
using namespace std;

const int WIDTH = 800;
const int HEIGHT = 600;
const int MOVIE_FPS = 24;

int currentUniverse = 1;

struct alignas(16) Camera {
    vec3 position;
    float _pad1;
    vec3 target;
    float _pad2;
    vec3 up;
    float _pad3;
    float fov;
    float azimuth, elevation, radius;
    bool dragging = false;
    bool panning = false;
    float lastX = 0, lastY = 0;

    Camera() : position(0, 0, 120.0f), target(0, 0, 0), up(0, 1, 0), 
               fov(60.0f), azimuth((float)M_PI / 4.0f), elevation(1.1f), radius(120.0f) {
        updatePosition();
    }

    void updatePosition() {
        position.x = target.x + radius * sin(elevation) * cos(azimuth);
        position.y = target.y + radius * cos(elevation);
        position.z = target.z + radius * sin(elevation) * sin(azimuth);
    }

    void orbit(float dx, float dy) {
        azimuth -= dx * 0.01f;
        elevation += dy * 0.01f;
        elevation = glm::clamp(elevation, 0.1f, (float)M_PI - 0.1f);
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
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera.position -= localUp * cameraSpeed;
        camera.target -= localUp * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        camera.position += localUp * cameraSpeed;
        camera.target += localUp * cameraSpeed;
    }
}

struct alignas(16) Sphere {
    vec4 centerAndRadius;
    vec4 color;
    vec4 properties;
    
    Sphere(vec3 c, float r, vec3 col, bool emissive = false, int universe = 1) {
        centerAndRadius = vec4(c, r);
        color = vec4(col, 1.0);
        properties = vec4(emissive ? 1.0f : 0.0f, float(universe), 0.0f, 0.0f);
    }
};

struct alignas(16) Star {
    vec4 data;
    vec4 colorAndSize;
};

struct Orbit {
    int parentIndex;      // -1: orbit around universe's sun at origin, >= 0: orbit around spheres[parentIndex] (moon)
    int universe;         // 1 or 2, ths determines universe of the object
    float radius;         // orbital radius from parent
    float angularSpeed;   // radians per second
    float inclinationDeg; // orbital plane tilt in degrees
    float phaseDeg;       // initial phase offset in degrees
};

vector<Sphere> spheres;
vector<Star> stars;
vector<Orbit> orbits;

void generateStars(int count) {
    for (int i = 0; i < count; i++) {
        vec3 dir = normalize(vec3(
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            (rand() / (float)RAND_MAX) * 2.0f - 1.0f
        ));
        float brightness = (rand() / (float)RAND_MAX) * 0.5f + 0.5f;
        float size = (rand() / (float)RAND_MAX) * 0.005f + 0.001f;

        float temp = (rand() / (float)RAND_MAX);
        vec3 color;
        if (temp < 0.33f) {
            color = vec3(0.8, 0.8, 1.0); // bluish
        } else if (temp < 0.66f) {
            color = vec3(1.0, 1.0, 1.0); // white
        } else {
            color = vec3(1.0, 1.0, 0.8); // yellowish
        }

        stars.push_back({vec4(dir, brightness), vec4(color, size)});
    }
}

// Helper fns, returns index of sun or planet

// Add a sun at origin (0,0,0) for the given universe
static int addSun(int universe, float radius, vec3 color) {
    spheres.push_back(Sphere(vec3(0, 0, 0), radius, color, true, universe));
    orbits.push_back({-2, universe, 0.0f, 0.0f, 0.0f, 0.0f}); // parentIndex -2: no orbit (sun)
    return (int)spheres.size() - 1;
}

// Add a planet orbiting its universe's sun, and computes an initial position
static int addPlanet(int universe, float orbitRadius, float angularSpeed, 
                     float inclinationDeg, float phaseDeg, float radius, vec3 color) {
    float angle = radians(phaseDeg);
    vec3 pos = vec3(orbitRadius * cos(angle), 0.0f, orbitRadius * sin(angle));
    mat4 inc = rotate(mat4(1.0f), radians(inclinationDeg), vec3(1, 0, 0));
    pos = vec3(inc * vec4(pos, 1.0f));
    
    spheres.push_back(Sphere(pos, radius, color, false, universe));
    orbits.push_back({-1, universe, orbitRadius, angularSpeed, inclinationDeg, phaseDeg}); // parentIndex -1: orbit sun
    return (int)spheres.size() - 1;
}

// Add a moon orbiting the given parent planet, and comptes an initial position
static int addMoon(int parentIndex, int universe, float orbitRadius, float angularSpeed,
                   float inclinationDeg, float phaseDeg, float radius, vec3 color) {
    vec3 parentPos = vec3(spheres[parentIndex].centerAndRadius);
    
    float angle = radians(phaseDeg);
    vec3 relPos = vec3(orbitRadius * cos(angle), 0.0f, orbitRadius * sin(angle));
    mat4 inc = rotate(mat4(1.0f), radians(inclinationDeg), vec3(1, 0, 0));
    relPos = vec3(inc * vec4(relPos, 1.0f));
    
    vec3 pos = parentPos + relPos;
    
    spheres.push_back(Sphere(pos, radius, color, false, universe));
    orbits.push_back({parentIndex, universe, orbitRadius, angularSpeed, inclinationDeg, phaseDeg});
    return (int)spheres.size() - 1;
}

struct Engine {
    GLFWwindow* window;
    GLuint quadVAO, quadVBO;
    GLuint texture;
    GLuint shaderProgram;

    GLuint computeShaderProgram;
    GLuint cameraUBO;
    GLuint spheresSSBO;
    GLuint starsSSBO;
    int renderWidth;
    int renderHeight;
    
    Engine() {
        initGLFW();
        initShaders();
        initQuad();
        initCompute();
    }
    
    void initGLFW() {
        if (!glfwInit()) {
            cerr << "failed to initialize glfw\n";
            exit(EXIT_FAILURE);
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        window = glfwCreateWindow(WIDTH, HEIGHT, "Wormhole Simulation", nullptr, nullptr);
        if (!window) {
            cerr << "failed to create window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        
        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);
        glewExperimental = GL_TRUE;
        
        if (glewInit() != GLEW_OK) {
            cerr << "failed to initialize glew\n";
            exit(EXIT_FAILURE);
        }
        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        renderWidth = (fbw > 0) ? fbw : WIDTH;
        renderHeight = (fbh > 0) ? fbh : HEIGHT;
        glViewport(0, 0, renderWidth, renderHeight);

        cout << "opengl " << glGetString(GL_VERSION) << "\n";
    }
    
    void initShaders() {  // glsl 3.30 core specified here
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
    
    string readShaderFromFile(const string& path) {
        ifstream file(path);
        if (!file.is_open()) {
            cerr << "error: could not open shader file: " << path << endl;
            return "";
        }
        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void initCompute() {
        string computeShaderSource = readShaderFromFile("wormhole.comp");
        const char* css_c = computeShaderSource.c_str();

        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &css_c, NULL);
        glCompileShader(computeShader);

        int success;
        char infoLog[1024];
        glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(computeShader, 1024, NULL, infoLog);
            cerr << "error: compute shader compilation failed\n" << infoLog << endl;
        }

        computeShaderProgram = glCreateProgram();
        glAttachShader(computeShaderProgram, computeShader);
        glLinkProgram(computeShaderProgram);

        glGetProgramiv(computeShaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(computeShaderProgram, 1024, NULL, infoLog);
            cerr << "error: compute shader linking failed\n" << infoLog << endl;
        }
        glDeleteShader(computeShader);

        glGenBuffers(1, &cameraUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Camera), NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);

        glGenBuffers(1, &spheresSSBO);
        glGenBuffers(1, &starsSSBO);
    }

    void uploadSceneData() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, spheresSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spheresSSBO);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, starsSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, stars.size() * sizeof(Star), stars.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, starsSSBO);
    }

    void updateSpheresSSBO() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, spheresSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, spheres.size() * sizeof(Sphere), spheres.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    void initQuad() {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,  1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    }

    void resize(int width, int height) {
        if (width <= 0 || height <= 0) return;
        renderWidth = width;
        renderHeight = height;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    }
    
    void computePixels() {
        glUseProgram(computeShaderProgram);

        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Camera), &camera);
        
        vec3 sunPosU1 = vec3(0, 5000, -6000); 
        vec3 sunColorU1 = vec3(1.0f, 0.9f, 0.7f);
        vec3 sunPosU2 = vec3(0, -7000, 8000);
        vec3 sunColorU2 = vec3(0.7f, 0.8f, 1.0f);

        for(const auto& s : spheres) {
            if (s.properties.x > 0.5f) { // isEmissive
                if (s.properties.y > 1.5f) { // universeID == 2
                    sunPosU2 = vec3(s.centerAndRadius);
                    sunColorU2 = vec3(s.color);
                } else { // universeID == 1
                    sunPosU1 = vec3(s.centerAndRadius);
                    sunColorU1 = vec3(s.color);
                }
            }
        }

        glUniform1i(glGetUniformLocation(computeShaderProgram, "currentUniverse"), currentUniverse);
        glUniform1i(glGetUniformLocation(computeShaderProgram, "numSpheres"), (GLint)spheres.size());
        glUniform1i(glGetUniformLocation(computeShaderProgram, "numStars"), (GLint)stars.size());
        glUniform3fv(glGetUniformLocation(computeShaderProgram, "sunPosU1"), 1, value_ptr(sunPosU1));
        glUniform3fv(glGetUniformLocation(computeShaderProgram, "sunPosU2"), 1, value_ptr(sunPosU2));
        glUniform3fv(glGetUniformLocation(computeShaderProgram, "sunColorU1"), 1, value_ptr(sunColorU1));
        glUniform3fv(glGetUniformLocation(computeShaderProgram, "sunColorU2"), 1, value_ptr(sunColorU2));
        glUniform1f(glGetUniformLocation(computeShaderProgram, "time"), (float)glfwGetTime());
        
        vec3 wormholeCenter = vec3(60.0f, 0.0f, -10.0f);
        float wormholeRadius = 6.0f;
        glUniform3fv(glGetUniformLocation(computeShaderProgram, "wormholeCenter"), 1, value_ptr(wormholeCenter));
        glUniform1f(glGetUniformLocation(computeShaderProgram, "wormholeRadius"), wormholeRadius);
        
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        int groupsX = (renderWidth + 7) / 8;
        int groupsY = (renderHeight + 7) / 8;
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void drawPixels() {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
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

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_U && action == GLFW_PRESS) {
        currentUniverse = (currentUniverse == 1) ? 2 : 1;
        cout << "switched to universe " << currentUniverse << "\n";
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            camera.dragging = true;
            camera.panning = (mods & GLFW_MOD_SHIFT);
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            camera.lastX = (float)x;
            camera.lastY = (float)y;
        } else {
            camera.dragging = false;
            camera.panning = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    if (camera.dragging) {
        float dx = (float)x - camera.lastX;
        float dy = (float)y - camera.lastY;
        if (camera.panning) {
            camera.pan(dx, dy);
        } else {
            camera.orbit(dx, dy);
        }
        camera.lastX = (float)x;
        camera.lastY = (float)y;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.zoom((float)yoffset);
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) {
        engine->resize(width, height);
    }
}

struct Keyframe {
    float timeSec;
    float posAzimuthDeg;
    float posElevationDeg;
    float posRadius;
    vec3 target;
};

static bool loadCameraPath(const string& pathFile, vector<Keyframe>& out) {
    ifstream in(pathFile);
    if (!in.is_open()) return false;
    string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        Keyframe k{};
        if (!(ss >> k.timeSec >> k.posAzimuthDeg >> k.posElevationDeg >> k.posRadius >> k.target.x >> k.target.y >> k.target.z)) continue;
        out.push_back(k);
    }
    return !out.empty();
}

static void setCamera(const vec3& pos, const vec3& target) {
    camera.position = pos;
    camera.target = target;
    vec3 dir = target - pos;
    camera.radius = length(dir);
    camera.azimuth = atan2(dir.z, dir.x);
    camera.elevation = acos(dir.y / camera.radius);
}

void updateOrbitalPositions(double time) {
    // First pass: update all bodies orbiting suns (parentIndex == -1)
    for (size_t i = 0; i < spheres.size(); ++i) {
        if (i >= orbits.size()) continue;
        
        const Orbit& orbit = orbits[i];
        if (orbit.parentIndex != -1) continue;
        
        // Find parent (sun) position for this universe
        vec3 parentPos = vec3(0.0f);
        for (size_t j = 0; j < spheres.size(); ++j) {
            bool isSun = spheres[j].properties.x > 0.5f;
            int sunUniverse = int(spheres[j].properties.y);
            if (isSun && sunUniverse == orbit.universe) {
                parentPos = vec3(spheres[j].centerAndRadius);
                break;
            }
        }
        
        // Calculate orbital position
        float angle = radians(orbit.phaseDeg) + orbit.angularSpeed * (float)time;
        mat4 inclinationMat = rotate(mat4(1.0f), radians(orbit.inclinationDeg), vec3(1.0f, 0.0f, 0.0f));
        vec3 orbitPos = vec3(orbit.radius * cos(angle), 0.0f, orbit.radius * sin(angle));
        orbitPos = vec3(inclinationMat * vec4(orbitPos, 1.0f));
        
        vec3 finalPos = parentPos + orbitPos;
        spheres[i].centerAndRadius = vec4(finalPos, spheres[i].centerAndRadius.w);
    }
    
    // Second pass: update all moons (parentIndex >= 0)
    for (size_t i = 0; i < spheres.size(); ++i) {
        if (i >= orbits.size()) continue;
        
        const Orbit& orbit = orbits[i];
        if (orbit.parentIndex < 0) continue;
        
        // Get parent planet position
        vec3 parentPos = vec3(spheres[orbit.parentIndex].centerAndRadius);
        
        // Calculate orbital position
        float angle = radians(orbit.phaseDeg) + orbit.angularSpeed * (float)time;
        mat4 inclinationMat = rotate(mat4(1.0f), radians(orbit.inclinationDeg), vec3(1.0f, 0.0f, 0.0f));
        vec3 orbitPos = vec3(orbit.radius * cos(angle), 0.0f, orbit.radius * sin(angle));
        orbitPos = vec3(inclinationMat * vec4(orbitPos, 1.0f));
        
        vec3 finalPos = parentPos + orbitPos;
        spheres[i].centerAndRadius = vec4(finalPos, spheres[i].centerAndRadius.w);
    }
}

void runInteractiveMode(Engine& engine) {
    cout << "starting interactive mode... (use -p for movie mode)\n";
    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(engine.window)) {
        processInput(engine.window);

        double time = glfwGetTime();
        updateOrbitalPositions(time);
        engine.updateSpheresSSBO();

        engine.render();
    
        frameCount++;
        double currentTime = glfwGetTime();
        double elapsedTime = currentTime - lastTime;
        if (elapsedTime >= 1.0) {
            double fps = double(frameCount) / elapsedTime;
            stringstream ss;
            ss << "wormhole | " << spheres.size() << " objects | " << fixed << setprecision(1) << fps << " fps";
            glfwSetWindowTitle(engine.window, ss.str().c_str());
            
            frameCount = 0;
            lastTime = currentTime;
        }
    }
}

void runMovieMode(Engine& engine) {
    cout << "movie mode: rendering frames from camera_path.txt...\n";
    vector<Keyframe> keys;
    if (!loadCameraPath("camera_path.txt", keys)) {
        cout << "error: camera_path.txt not found or invalid.\n";
        return;
    }

    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    tm timeinfo;
    localtime_s(&timeinfo, &in_time_t);
    ss << "exports/run_" << put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S");
    string exportDir = ss.str();
    filesystem::create_directories(exportDir);

    float totalDuration = keys.back().timeSec;
    int totalFrames = static_cast<int>(totalDuration * MOVIE_FPS);
    
    cout << "rendering " << totalFrames << " frames for a " << totalDuration << "s video...\n";

    // For movie mode, lock the render resolution to the base size to keep all frames consistent
    engine.resize(WIDTH, HEIGHT);

    size_t keyframe_idx = 0;
    for (int i = 0; i < totalFrames; ++i) {
        float currentTime = static_cast<float>(i) / MOVIE_FPS;
        
        // Update orbital motion for this frame
        updateOrbitalPositions(currentTime);
        engine.updateSpheresSSBO();

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
        ostringstream name;
        name << exportDir << "/frame_" << setw(5) << setfill('0') << i << ".png";
        string file = name.str();

        int w = engine.renderWidth;
        int h = engine.renderHeight;

        vector<float> gpu_pixels(w * h * 4);
        glBindTexture(GL_TEXTURE_2D, engine.texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, gpu_pixels.data());

        vector<unsigned char> png_pixels(w * h * 3);
        for(int y = 0; y < h; ++y) {
            for(int x = 0; x < w; ++x) {
                int flipped_y = h - 1 - y;
                int gpu_idx = (y * w + x) * 4;
                int png_idx = (flipped_y * w + x) * 3;
                png_pixels[png_idx + 0] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 0], 0.0f, 1.0f) * 255);
                png_pixels[png_idx + 1] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 1], 0.0f, 1.0f) * 255);
                png_pixels[png_idx + 2] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 2], 0.0f, 1.0f) * 255);
            }
        }
        stbi_write_png(file.c_str(), w, h, 3, png_pixels.data(), w * 3);
        
        cout << "saved frame " << (i + 1) << "/" << totalFrames << "\r" << flush;
    }
    cout << "\nrender complete: " << totalFrames << " frames written to " << exportDir << "/.\n";

    cout << "running ffmpeg to create video...\n";
    string videoFile = ss.str() + ".mp4";
    string ffmpeg_cmd = "ffmpeg -r " + to_string(MOVIE_FPS) 
                           + " -i " + exportDir + "/frame_%05d.png"
                           + " -c:v libx264 -pix_fmt yuv420p -y " + videoFile;

    int ffmpeg_ret = system(ffmpeg_cmd.c_str());
    if (ffmpeg_ret == 0) {
        cout << "successfully created video: " << videoFile << "\n";
    } else {
        cout << "error: ffmpeg command failed. you can try running it manually:\n" << ffmpeg_cmd << "\n";
    }
}

int main(int argc, char** argv) {
    bool predefinedPath = false;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--predefined" || a == "-p") predefinedPath = true;
    }
    Engine engine;
    
    glfwSetKeyCallback(engine.window, keyCallback);
    glfwSetMouseButtonCallback(engine.window, mouseButtonCallback);
    glfwSetCursorPosCallback(engine.window, cursorPosCallback);
    glfwSetScrollCallback(engine.window, scrollCallback);
    glfwSetFramebufferSizeCallback(engine.window, framebufferSizeCallback);
    
    cout << "\nwormhole simulation\n\n";

    // Build two compact solar systems using helper fns
    spheres.clear();
    orbits.clear();
    spheres.reserve(32);
    orbits.reserve(32);

    // Universe 1: Yellow sun with 4 planets and 3 moons
    int u1_sun = addSun(1, 5.0f, vec3(1.0f, 0.9f, 0.7f));
    int u1_p1 = addPlanet(1, 12.0f, 0.35f, 1.5f, 0.0f, 0.9f, vec3(0.95f, 0.4f, 0.25f));
    addMoon(u1_p1, 1, 1.5f, 1.2f, 5.0f, 45.0f, 0.30f, vec3(0.7f, 0.7f, 0.7f));
    int u1_p2 = addPlanet(1, 22.0f, 0.22f, 3.5f, 90.0f, 1.2f, vec3(0.3f, 0.8f, 0.3f));
    addMoon(u1_p2, 1, 2.0f, 0.9f, 8.0f, 120.0f, 0.40f, vec3(0.6f, 0.6f, 0.6f));
    int u1_p3 = addPlanet(1, 35.0f, 0.14f, 6.0f, 180.0f, 1.5f, vec3(0.3f, 0.4f, 0.9f));
    addMoon(u1_p3, 1, 2.5f, 0.75f, 12.0f, 210.0f, 0.35f, vec3(0.8f, 0.7f, 0.6f));
    int u1_p4 = addPlanet(1, 48.0f, 0.09f, 2.5f, 270.0f, 1.8f, vec3(0.9f, 0.6f, 0.2f));

    // Universe 2: Blue sun with 4 planets and 2 moons
    int u2_sun = addSun(2, 5.5f, vec3(0.7f, 0.8f, 1.0f));
    int u2_p1 = addPlanet(2, 15.0f, 0.30f, 2.0f, 30.0f, 0.7f, vec3(0.9f, 0.9f, 0.3f));
    int u2_p2 = addPlanet(2, 25.0f, 0.20f, 4.5f, 120.0f, 1.0f, vec3(0.9f, 0.3f, 0.8f));
    addMoon(u2_p2, 2, 1.8f, 1.0f, 7.0f, 60.0f, 0.25f, vec3(0.5f, 0.5f, 0.5f));
    int u2_p3 = addPlanet(2, 38.0f, 0.13f, 7.5f, 200.0f, 1.4f, vec3(0.3f, 0.9f, 0.9f));
    addMoon(u2_p3, 2, 2.2f, 0.8f, 10.0f, 150.0f, 0.40f, vec3(0.7f, 0.6f, 0.5f));
    int u2_p4 = addPlanet(2, 50.0f, 0.08f, 3.0f, 300.0f, 2.0f, vec3(0.9f, 0.9f, 0.9f));

    currentUniverse = 1;
    
    generateStars(1000);
    engine.uploadSceneData();

    if (predefinedPath) {
        runMovieMode(engine);
    } else {
        runInteractiveMode(engine);
    }

    cout << "\nsimulation ended.\n";
    glfwTerminate();
    return 0;
}
