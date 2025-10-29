#ifdef _WIN32
#include <windows.h>
#endif

#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef _OPENMP
#include <omp.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

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
const int SAMPLES_PER_PIXEL = 4; // 2x2 supersampling for antialiasing

const float THROAT_RADIUS = 25.0f;
const vec3 THROAT_CENTER = vec3(0, 0, 0);

//------------------------------------------------------------------------------
// camera
//------------------------------------------------------------------------------
struct Camera {
    vec3 position;
    vec3 target;
    vec3 up;
    float fov;
};

//------------------------------------------------------------------------------
// scene objects
//------------------------------------------------------------------------------
struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
    bool isEmissive;
    int universeID;
};

vector<Sphere> spheres;

//------------------------------------------------------------------------------
// physics and ray tracing
//------------------------------------------------------------------------------
struct GeodesicRay {
    vec3 pos; // cartesian position
    vec3 dir; // cartesian direction
    int universe;

    // state for RK4 integrator (cylindrical coordinates for wormhole metric)
    float l;  // proper radial distance
    float rho; // cylindrical radius sqrt(x^2 + z^2)
    float phi; // cylindrical angle
    
    vec3 momentum; // p_l, p_rho, p_phi
};

struct HitInfo {
    bool hit;
    float distance;
    vec3 normal;
    vec3 color;
    bool isEmissive;
};

// Simplified ray-sphere intersection for objects (not for wormhole)
HitInfo intersectScene(const vec3& origin, const vec3& direction, int universe) {
    HitInfo closestHit;
    closestHit.hit = false;
    closestHit.distance = 1e10;

    for (const auto& sphere : spheres) {
        if (sphere.universeID == universe) {
            vec3 oc = origin - sphere.center;
            float a = dot(direction, direction);
            float b = 2.0f * dot(oc, direction);
            float c = dot(oc, oc) - sphere.radius * sphere.radius;
            float discriminant = b * b - 4 * a * c;

            if (discriminant >= 0) {
                float t = (-b - sqrt(discriminant)) / (2.0f * a);
                if (t > 0.001f && t < closestHit.distance) {
                    closestHit.hit = true;
                    closestHit.distance = t;
                    closestHit.normal = normalize((origin + direction * t) - sphere.center);
                    closestHit.color = sphere.color;
                    closestHit.isEmissive = sphere.isEmissive;
                }
            }
        }
    }
    return closestHit;
}

// convert cartesian coordinates to cylindrical for the wormhole metric
void cartesianToCylindrical(const vec3& pos, float& l, float& rho, float& phi) {
    rho = sqrt(pos.x * pos.x + pos.z * pos.z);
    phi = atan2(pos.z, pos.x);
    // for the Morris-Thorne metric, l is defined by this integral,
    // but we can approximate it or use it as a measure of progress.
    // here, we'll use y as the equivalent of 'l' for simplicity,
    // assuming the wormhole is aligned along the y-axis.
    l = pos.y;
}

vec3 cylindricalToCartesian(float l, float rho, float phi) {
    return vec3(rho * cos(phi), l, rho * sin(phi));
}

// calculates the derivatives for the geodesic equations (the core of the physics)
GeodesicRay derivatives(const GeodesicRay& ray) {
    GeodesicRay derivs;
    float r = sqrt(ray.l * ray.l + THROAT_RADIUS * THROAT_RADIUS);
    
    // these are derived from the christoffel symbols for the morris-thorne metric
    derivs.pos.x = ray.momentum.x;
    derivs.pos.y = ray.momentum.y;
    derivs.pos.z = ray.momentum.z;

    float r3 = r * r * r;
    derivs.momentum.x = -(THROAT_RADIUS * THROAT_RADIUS * ray.l / r3) * ray.pos.x;
    derivs.momentum.y = -(THROAT_RADIUS * THROAT_RADIUS * ray.l / r3) * ray.pos.y;
    derivs.momentum.z = -(THROAT_RADIUS * THROAT_RADIUS * ray.l / r3) * ray.pos.z;
    
    return derivs;
}

// this is the heart of the new physics engine
vec3 traceRay(const vec3& origin, const vec3& direction) {
    // first, check for simple intersections in the local universe
    HitInfo sceneHit = intersectScene(origin, direction, 1);
    
    GeodesicRay geoRay;
    geoRay.pos = origin;
    geoRay.dir = direction;
    geoRay.universe = 1;
    geoRay.momentum = direction; // initial momentum is just the direction

    const float step_size = 0.5f;
    const int max_steps = 1000;

    for (int i = 0; i < max_steps; ++i) {
        // check for intersection before taking a step
        HitInfo hit = intersectScene(geoRay.pos, normalize(geoRay.momentum), geoRay.universe);
        if (hit.hit && hit.distance < step_size * 2.0f) {
            if (hit.isEmissive) {
                return hit.color;
            }
            // simple lambertian lighting
            vec3 sunPos = (geoRay.universe == 1) ? spheres[0].center : spheres[3].center;
            vec3 lightDir = normalize(sunPos - (geoRay.pos + normalize(geoRay.momentum) * hit.distance));
            float diffuse = std::max(0.0f, dot(hit.normal, lightDir));
            return hit.color * (0.2f + 0.8f * diffuse);
        }

        // RK4 integration step
        GeodesicRay k1 = derivatives(geoRay);
        
        GeodesicRay temp;
        temp.pos = geoRay.pos + k1.pos * (step_size / 2.0f);
        temp.momentum = geoRay.momentum + k1.momentum * (step_size / 2.0f);
        GeodesicRay k2 = derivatives(temp);

        temp.pos = geoRay.pos + k2.pos * (step_size / 2.0f);
        temp.momentum = geoRay.momentum + k2.momentum * (step_size / 2.0f);
        GeodesicRay k3 = derivatives(temp);

        temp.pos = geoRay.pos + k3.pos * step_size;
        temp.momentum = geoRay.momentum + k3.momentum * step_size;
        GeodesicRay k4 = derivatives(temp);

        geoRay.pos += (k1.pos + 2.0f * k2.pos + 2.0f * k3.pos + k4.pos) * (step_size / 6.0f);
        geoRay.momentum += (k1.momentum + 2.0f * k2.momentum + 2.0f * k3.momentum + k4.momentum) * (step_size / 6.0f);

        // check for wormhole traversal
        float dist_from_center_sq = dot(geoRay.pos, geoRay.pos);
        if (dist_from_center_sq < THROAT_RADIUS * THROAT_RADIUS) {
            geoRay.pos = -geoRay.pos; // teleport to the other side
            geoRay.universe = (geoRay.universe == 1) ? 2 : 1;
        }

        // escape condition
        if (dist_from_center_sq > 20000.0f * 20000.0f) {
            break; // ray has flown off to infinity
        }
    }

    // if we didn't hit anything, return a background color
    return vec3(0.0, 0.0, 0.0); // pitch black space
}


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
int main(int argc, char** argv) {
    cout << "\nwormhole geodesic renderer (physically accurate)\n";
    cout << "this will be very slow. rendering one frame...\n";

    // scene setup
    spheres.push_back({vec3(0, 5000, -6000), 1000, vec3(1.0f, 0.9f, 0.7f), true, 1});
    spheres.push_back({vec3(-80, 40, 0), 10, vec3(1.0f, 0.2f, 0.2f), false, 1});
    spheres.push_back({vec3(-120, 0, 0), 12, vec3(1.0f, 0.5f, 0.0f), false, 1});
    
    spheres.push_back({vec3(0, -7000, 8000), 1500, vec3(0.7f, 0.8f, 1.0f), true, 2});
    spheres.push_back({vec3(80, 40, 0), 18, vec3(1.0f, 1.0f, 0.2f), false, 2});
    spheres.push_back({vec3(120, 0, 0), 22, vec3(1.0f, 1.0f, 1.0f), false, 2});

    // camera setup
    Camera camera;
    camera.position = vec3(0, 0, 80);
    camera.target = vec3(0, 0, 0);
    camera.up = vec3(0, 1, 0);
    camera.fov = 60.0f;

    vector<unsigned char> pixels(WIDTH * HEIGHT * 3);
    
    auto t_start = chrono::high_resolution_clock::now();

    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < HEIGHT; ++y) {
        if (omp_get_thread_num() == 0) {
            cout << "rendering scanline " << y << "/" << HEIGHT << "\r" << flush;
        }
        for (int x = 0; x < WIDTH; ++x) {
            vec3 final_color(0.0f);

            // supersampling for antialiasing
            for (int s = 0; s < SAMPLES_PER_PIXEL; ++s) {
                float u = (float(x) + (float(s % 2) + 0.5f) / 2.0f) / float(WIDTH);
                float v = (float(y) + (float(s / 2) + 0.5f) / 2.0f) / float(HEIGHT);

                vec3 forward = normalize(camera.target - camera.position);
                vec3 right = normalize(cross(forward, camera.up));
                vec3 up = cross(right, forward);

                float aspect = (float)WIDTH / (float)HEIGHT;
                float tanHalfFov = tan(radians(camera.fov) * 0.5f);
                
                float Px = (2.0f * u - 1.0f) * aspect * tanHalfFov;
                float Py = (1.0f - 2.0f * v) * tanHalfFov;

                vec3 rayDir = normalize(Px * right + Py * up + forward);
                final_color += traceRay(camera.position, rayDir);
            }
            final_color /= (float)SAMPLES_PER_PIXEL;
            
            int index = (y * WIDTH + x) * 3;
            pixels[index + 0] = static_cast<unsigned char>(glm::clamp(final_color.r, 0.0f, 1.0f) * 255);
            pixels[index + 1] = static_cast<unsigned char>(glm::clamp(final_color.g, 0.0f, 1.0f) * 255);
            pixels[index + 2] = static_cast<unsigned char>(glm::clamp(final_color.b, 0.0f, 1.0f) * 255);
        }
    }

    auto t_end = chrono::high_resolution_clock::now();
    double elapsed_time_s = chrono::duration<double>(t_end - t_start).count();
    cout << "\nrender finished in " << fixed << setprecision(2) << elapsed_time_s << " seconds.\n";

    // save the final image
    std::filesystem::create_directories("exports");
    string filename = "exports/wormhole_geodesic_render.png";
    int success = stbi_write_png(filename.c_str(), WIDTH, HEIGHT, 3, pixels.data(), WIDTH * 3);
    
    if (success) {
        cout << "image saved to " << filename << "\n";
    } else {
        cerr << "error: failed to save image to " << filename << "\n";
    }

    return 0;
}
