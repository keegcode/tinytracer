#include <SDL3/SDL.h>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <random>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef _MSC_VER
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() asm("int $3")
#endif

#define SDL_ASSERT(expr)                                                       \
  if (expr) {                                                                  \
  } else {                                                                     \
    fmt::println("Assertion {} failed.\nError: {}\n", #expr, SDL_GetError());  \
    DEBUG_BREAK();                                                             \
  }

float randomFloat();
float randomFloat(float min, float max);

glm::vec3 randomVec3();
glm::vec3 randomVec3(float min, float max);

glm::vec2 randomVec2(float min, float max);
glm::vec2 randomVec2();

glm::vec3 randomUnitVec3OnSphere();

glm::vec2 pixelToWorld(float pixelX, float pixelY);

bool nearZero(const glm::vec3& vec);

struct Material {
  glm::vec3 albedo;
  float roughness;
  float metallic;
};

struct Sphere {
  glm::vec3 center;
  float radius;
  Material material;
};

struct Camera {
  glm::vec3 position;
};

struct HitRecord {
  Sphere *sphere;
  float t;
};

struct Ray {
  glm::vec3 origin;
  glm::vec3 direction;

  glm::vec3 at(float t) const { return origin + t * direction; }

  Ray scatter(const glm::vec3& p, const glm::vec3& n, const Material& mat) const {
    if (mat.metallic) {
      return Ray{p, glm::reflect(p - origin, n)};
    }

    glm::vec3 dir = randomUnitVec3OnSphere();

    if (nearZero(n + dir)) {
      dir = n;
    }

    return Ray{p, n + dir};
  }

  float intersects(const Sphere &sphere, float minT, float maxT) const {
    glm::vec3 oc = sphere.center - origin;
    float a = glm::dot(direction, direction);
    float h = glm::dot(direction, oc);
    float c = glm::dot(oc, oc) - (sphere.radius * sphere.radius);
    float d = h * h - a * c;

    if (d < 0) {
      return -1.0f;
    }

    d = glm::sqrt(d);

    float t = (h - d) / a;

    if (t <= minT || t >= maxT) {
      t = (h + d) / a;
      if (t <= minT || t >= maxT) {
        return -1.0f;
      }
    }

    return t;
  }
};

struct World {
  Camera camera;
  std::vector<Sphere> spheres;

  glm::vec3 color(const Ray &ray, float depth) {
    if (depth <= 0) {
      return glm::vec3{0.0};
    }

    HitRecord record = hit(ray);
    Sphere *sphere = record.sphere;

    if (sphere == nullptr) {
      return glm::vec3{0.5, 0.8, 0.9};
    }

    glm::vec3 p = ray.at(record.t);
    glm::vec3 n = (p - sphere->center) / sphere->radius;

    Material &mat = sphere->material;
    Ray scattered = ray.scatter(p, n, mat);

    return 0.25f * mat.albedo * color(scattered, depth - 1);
  }

  HitRecord hit(const Ray &ray) {
    float closestT = std::numeric_limits<float>::max();
    Sphere *closestSphere = nullptr;

    for (Sphere &sphere : spheres) {
      float t =
          ray.intersects(sphere, 0.001, std::numeric_limits<float>::infinity());

      if (t < 0 || t > closestT) {
        continue;
      }

      closestT = t;
      closestSphere = &sphere;
    }

    return {closestSphere, closestT};
  };
};

float fov = glm::tan(glm::radians(60.0f / 2.0f));
float sampleCount = 150.0f;
float rayDepth = 50.0f;

uint32_t w, h;
float aspectRatio;

World world{.camera = {.position = glm::vec3{0.0f}},
            .spheres = {
                {.center = glm::vec3{0.0f, 0.0f, -1.0},
                 .radius = 0.2f,
                 .material = {.albedo = glm::vec3{0.5, 0.5, 0.5},
                              .roughness = 1.0f,
                              .metallic = 0.0f}},
                {.center = glm::vec3{0.45f, 0.0f, -1.0},
                 .radius = 0.2f,
                 .material = {.albedo = glm::vec3{1.0, 1.0, 1.0},
                              .roughness = 1.0f,
                              .metallic = 1.0f}},
                {.center = glm::vec3{0.0f, -100.21f, -1.0},
                 .radius = 100.0f,
                 .material = {.albedo = glm::vec3{0.4, 0.8, 0.5},
                              .roughness = 1.0f,
                              .metallic = 0.0f}},
            }};

int main() {
  bool isRunning = true;

  SDL_ASSERT(SDL_Init(SDL_INIT_VIDEO));

  int displaysCount;
  SDL_DisplayID *displays = SDL_GetDisplays(&displaysCount);
  SDL_ASSERT(displays != nullptr);

  const SDL_DisplayMode *displayMode = SDL_GetCurrentDisplayMode(displays[0]);
  SDL_ASSERT(displayMode != nullptr);

  w = displayMode->w / 3;
  h = displayMode->h / 3;

  aspectRatio = static_cast<float>(w) / h;

  SDL_Window *window =
      SDL_CreateWindow("tinytracer", static_cast<int>(w), static_cast<int>(h),
                       SDL_WINDOW_RESIZABLE);
  SDL_ASSERT(window != nullptr);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  SDL_ASSERT(renderer != nullptr);

  SDL_Texture *texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
      static_cast<int>(w), static_cast<int>(h));
  SDL_ASSERT(texture != nullptr);

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_SetTextureAlphaMod(texture, 255);

  std::vector<uint32_t> pixels(w * h);

  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      glm::vec3 pixelColor{0.0};
      uint32_t idx = y * w + x;

      Ray ray{};
      ray.origin = world.camera.position;

      for (uint32_t i = 0; i < sampleCount; i++) {
        glm::vec2 offset = randomVec2(-0.5f, 0.5f);
        glm::vec2 pos = pixelToWorld(static_cast<float>(x) + offset.x,
                                     static_cast<float>(y) + offset.y);
        ray.direction = glm::normalize(glm::vec3{pos, -1.0f} - ray.origin);
        pixelColor += world.color(ray, rayDepth);
      }

      pixelColor = glm::clamp(glm::sqrt(pixelColor / sampleCount), 0.0f, 1.0f);

      pixels[idx] = static_cast<uint32_t>(pixelColor.r * 255) << 24 |
                    static_cast<uint32_t>(pixelColor.g * 255) << 16 |
                    static_cast<uint32_t>(pixelColor.b * 255) << 8 | 0x000000FF;
    }
  }

  void *texturePixels;
  int pitch;
  SDL_LockTexture(texture, nullptr, &texturePixels, &pitch);
  memcpy(texturePixels, pixels.data(), pixels.size() * sizeof(uint32_t));
  SDL_UnlockTexture(texture);

  while (isRunning) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        isRunning = false;
      }
      if (event.type == SDL_EVENT_KEY_DOWN &&
          event.key.scancode == SDL_SCANCODE_ESCAPE) {
        isRunning = false;
      }
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    SDL_Delay(10);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();

  return 0;
}

float randomFloat() {
  static std::uniform_real_distribution<float> distribution(0.0, 1.0);
  static std::mt19937 generator;
  return distribution(generator);
}

float randomFloat(float min, float max) {
  return min + (max - min) * randomFloat();
}

glm::vec2 randomVec2() {
  return glm::vec2{
      randomFloat(),
      randomFloat(),
  };
}

glm::vec2 randomVec2(float min, float max) {
  return glm::vec2{randomFloat(min, max), randomFloat(min, max)};
}

glm::vec3 randomVec3() {
  return glm::vec3{randomFloat(), randomFloat(), randomFloat()};
}

glm::vec3 randomVec3(float min, float max) {
  return glm::vec3{randomFloat(min, max), randomFloat(min, max),
                   randomFloat(min, max)};
}

glm::vec3 randomUnitVec3OnSphere() {
  while (true) {
    glm::vec3 dir = randomVec3(-1.0f, 1.0f);
    float l = glm::dot(dir, dir);
    if (std::numeric_limits<float>::min() < l && l <= 1) {
      return dir / glm::sqrt(l);
    }
  }
}

glm::vec2 pixelToWorld(float x, float y) {
  return {(((2 * ((x + 0.5f) / w)) - 1.0f) * aspectRatio * fov),
          ((1.0 - (2 * ((y + 0.5f) / h))) * fov)};
}

bool nearZero(const glm::vec3& vec) {
  constexpr float e = std::numeric_limits<float>::epsilon();
  return glm::abs(vec.r) < e && glm::abs(vec.g) < e && glm::abs(vec.b) < e;
};
