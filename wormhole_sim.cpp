#ifdef _WIN32
#include <windows.h>
#endif

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

//------------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------------
const int WIDTH = 800;
const int HEIGHT = 600;
const int MOVIE_FPS = 24;

const float THROAT_RADIUS = 25.0f;
const vec3 THROAT_CENTER = vec3(0, 0, 0);
const float BENDING_STRENGTH = 0.95f;

int currentUniverse = 1;

//------------------------------------------------------------------------------
// camera
//------------------------------------------------------------------------------
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

    Camera() : position(0, 0, 80.0f), target(0, 0, 0), up(0, 1, 0), 
               fov(60.0f), azimuth(0), elevation((float)M_PI / 2.0f), radius(80.0f) {}

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

//------------------------------------------------------------------------------
// input handling
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// scene objects
//------------------------------------------------------------------------------
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

vector<Sphere> spheres;
vector<Star> stars;

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

//------------------------------------------------------------------------------
// gpu renderer
//------------------------------------------------------------------------------
struct Engine {
    GLFWwindow* window;
    GLuint quadVAO, quadVBO;
    GLuint texture;
    GLuint shaderProgram;
    vector<unsigned char> pixels;

    GLuint computeShaderProgram;
    GLuint cameraUBO;
    GLuint spheresSSBO;
    GLuint starsSSBO;
    
    Engine() {
        pixels.resize(WIDTH * HEIGHT * 3);
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
        glewExperimental = GL_TRUE;
        
        if (glewInit() != GLEW_OK) {
            cerr << "failed to initialize glew\n";
            exit(EXIT_FAILURE);
        }
        
        cout << "opengl " << glGetString(GL_VERSION) << "\n";
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
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
        
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(WIDTH / 8, HEIGHT / 8, 1);
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

//------------------------------------------------------------------------------
// callbacks and helpers
//------------------------------------------------------------------------------
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

static void writePPM(const string& filename, const vector<unsigned char>& buf, int w, int h) {
    ofstream out(filename, ios::binary);
    out << "P6\n" << w << " " << h << "\n255\n";
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<streamsize>(w*h*3));
}

//------------------------------------------------------------------------------
// main loop modes
//------------------------------------------------------------------------------
void runInteractiveMode(Engine& engine, const vector<Sphere>& initialSpheres) {
    cout << "starting interactive mode... (use -p for movie mode)\n";
    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(engine.window)) {
        processInput(engine.window);

        double time = glfwGetTime();
        for (size_t i = 0; i < spheres.size(); ++i) {
            bool isEmissive = initialSpheres[i].properties.x > 0.5f;
            if (!isEmissive) {
                vec3 initialPos = vec3(initialSpheres[i].centerAndRadius);
                
                int universeID = int(initialSpheres[i].properties.y);
                vec3 rotation_axis = (universeID == 1) ? vec3(0.0, 1.0, 0.0) : vec3(0.1, 1.0, 0.0);
                float orbit_radius = length(vec3(initialPos.x, 0.0, initialPos.z));
                float speed_factor = 150.0f;
                float angular_velocity = 10.0f / (orbit_radius + speed_factor);
                float angle = (float)time * angular_velocity;
                
                if (universeID == 2) {
                    angle = -angle;
                }

                glm::mat4 rotation_matrix = glm::rotate(glm::mat4(1.0f), angle, normalize(rotation_axis));
                vec3 new_pos = vec3(rotation_matrix * vec4(initialPos, 1.0f));
                
                spheres[i].centerAndRadius.x = new_pos.x;
                spheres[i].centerAndRadius.y = new_pos.y;
                spheres[i].centerAndRadius.z = new_pos.z;
            }
        }
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

    size_t keyframe_idx = 0;
    for (int i = 0; i < totalFrames; ++i) {
        float currentTime = static_cast<float>(i) / MOVIE_FPS;

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
        name << exportDir << "/frame_" << setw(5) << setfill('0') << i << ".ppm";
        string file = name.str();

        vector<float> gpu_pixels(WIDTH * HEIGHT * 4);
        glBindTexture(GL_TEXTURE_2D, engine.texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, gpu_pixels.data());

        vector<unsigned char> ppm_pixels(WIDTH * HEIGHT * 3);
        for(int y = 0; y < HEIGHT; ++y) {
            for(int x = 0; x < WIDTH; ++x) {
                int flipped_y = HEIGHT - 1 - y;
                int gpu_idx = (y * WIDTH + x) * 4;
                int ppm_idx = (flipped_y * WIDTH + x) * 3;
                ppm_pixels[ppm_idx + 0] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 0], 0.0f, 1.0f) * 255);
                ppm_pixels[ppm_idx + 1] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 1], 0.0f, 1.0f) * 255);
                ppm_pixels[ppm_idx + 2] = static_cast<unsigned char>(glm::clamp(gpu_pixels[gpu_idx + 2], 0.0f, 1.0f) * 255);
            }
        }
        writePPM(file, ppm_pixels, WIDTH, HEIGHT);
        
        cout << "saved frame " << (i + 1) << "/" << totalFrames << "\r" << flush;
    }
    cout << "\nrender complete: " << totalFrames << " frames written to " << exportDir << "/.\n";

    cout << "running ffmpeg to create video...\n";
    string videoFile = ss.str() + ".mp4";
    string ffmpeg_cmd = "ffmpeg -r " + to_string(MOVIE_FPS) 
                           + " -i " + exportDir + "/frame_%05d.ppm"
                           + " -c:v libx264 -pix_fmt yuv420p -y " + videoFile;

    int ffmpeg_ret = system(ffmpeg_cmd.c_str());
    if (ffmpeg_ret == 0) {
        cout << "successfully created video: " << videoFile << "\n";
    } else {
        cout << "error: ffmpeg command failed. you can try running it manually:\n" << ffmpeg_cmd << "\n";
    }
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
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
    
    cout << "\nwormhole simulation\n\n";

    spheres.push_back(Sphere(vec3(0, 5000, -6000), 1000, vec3(1.0f, 0.9f, 0.7f), true, 1));
    spheres.push_back(Sphere(vec3(-80, 40, 0), 10, vec3(1.0f, 0.2f, 0.2f), false, 1));
    spheres.push_back(Sphere(vec3(-80, -40, 0), 10, vec3(0.2f, 1.0f, 0.2f), false, 1));
    spheres.push_back(Sphere(vec3(-100, 0, 50), 10, vec3(0.2f, 0.2f, 1.0f), false, 1));
    spheres.push_back(Sphere(vec3(-120, 0, 0), 12, vec3(1.0f, 0.5f, 0.0f), false, 1));

    spheres.push_back(Sphere(vec3(0, -7000, 8000), 1500, vec3(0.7f, 0.8f, 1.0f), true, 2));
    spheres.push_back(Sphere(vec3(80, 40, 0), 18, vec3(1.0f, 1.0f, 0.2f), false, 2));
    spheres.push_back(Sphere(vec3(80, -40, 0), 18, vec3(1.0f, 0.2f, 1.0f), false, 2));
    spheres.push_back(Sphere(vec3(100, 0, 50), 18, vec3(0.2f, 1.0f, 1.0f), false, 2));
    spheres.push_back(Sphere(vec3(120, 0, 0), 22, vec3(1.0f, 1.0f, 1.0f), false, 2));
    
    currentUniverse = 1;
    
    generateStars(1000);
    engine.uploadSceneData();

    cout << "universe 1 has a yellow sun and " << 4 << " planets.\n";
    cout << "universe 2 has a blue sun and " << 4 << " planets.\n";
    
    const auto initialSpheres = spheres;

    if (predefinedPath) {
        runMovieMode(engine);
    } else {
        runInteractiveMode(engine, initialSpheres);
    }

    cout << "\nsimulation ended.\n";
    glfwTerminate();
    return 0;
}
