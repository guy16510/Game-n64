#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Jet.hpp"

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 172;

std::uint64_t fnv1a_pixels(const std::vector<std::uint16_t>& pixels) {
    std::uint64_t hash = 1469598103934665603ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    for (const std::uint16_t pixel : pixels) {
        hash ^= static_cast<std::uint8_t>(pixel & 0xFFU);
        hash *= kPrime;
        hash ^= static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
        hash *= kPrime;
    }
    return hash;
}

void write_ppm(const std::string& path, const std::vector<std::uint16_t>& pixels) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open render output");
    }

    file << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";

    for (const std::uint16_t pixel : pixels) {
        const std::uint8_t r5 = static_cast<std::uint8_t>((pixel >> 11U) & 0x1FU);
        const std::uint8_t g6 = static_cast<std::uint8_t>((pixel >> 5U) & 0x3FU);
        const std::uint8_t b5 = static_cast<std::uint8_t>(pixel & 0x1FU);

        const unsigned char rgb[3] = {
            static_cast<unsigned char>((static_cast<unsigned int>(r5) * 255U) / 31U),
            static_cast<unsigned char>((static_cast<unsigned int>(g6) * 255U) / 63U),
            static_cast<unsigned char>((static_cast<unsigned int>(b5) * 255U) / 31U),
        };
        file.write(reinterpret_cast<const char*>(rgb), 3);
    }
}

}  // namespace

int main(int argc, char** argv) {
    using namespace Renderer;

    const std::string output_path = argc > 1 ? argv[1] : "jet-smoke.ppm";

    std::vector<std::uint16_t> framebuffer(static_cast<std::size_t>(kWidth) *
                                           static_cast<std::size_t>(kHeight));
    std::vector<std::uint16_t> depth(static_cast<std::size_t>(ZBUFFER_STRIDE(kWidth)) *
                                     static_cast<std::size_t>(kHeight));

    Scene scene(framebuffer.data(), depth.data(), kWidth, kHeight);
    scene.setBackcolor(0x0000U);
    scene.setClearBuffer(true);

    Camera camera;
    camera.setPosition(0, 0, -520);
    camera.setRotation(0, 0, 0);
    camera.setFOV(72, kWidth);
    camera.nearPlane = 32;
    camera.farPlane = 4096;
    scene.setCamera(&camera);

    DirectionalLight sun(Vector3{35, -25, 0}, Color{255, 244, 220}, 230);
    AmbientLight ambient(Color{32, 42, 58});
    scene.setDirectionalLight(&sun);
    scene.setAmbientLight(&ambient);

    Material rock(0x8C71U);
    rock.shadingMode = ShadingMode::GOURAUD;
    rock.diffuse = 230;
    rock.specular = 12;

    Object* asteroid = Primitives::createSphere(115, 14, &rock);
    if (asteroid == nullptr) {
        std::cerr << "Jet failed to create sphere primitive\n";
        return 2;
    }

    asteroid->translate(0, 0, 360);
    asteroid->rotate(18, 31, 7);
    scene.addObject(asteroid);
    scene.render();

    // Deterministic star field added after the 3D pass. Only paint clear
    // pixels so the stars cannot overwrite the asteroid.
    std::uint32_t rng = 0x51A7C0DEU;
    for (int i = 0; i < 90; ++i) {
        rng ^= rng << 13U;
        rng ^= rng >> 17U;
        rng ^= rng << 5U;
        const int x = static_cast<int>(rng % static_cast<std::uint32_t>(kWidth));
        rng ^= rng << 13U;
        rng ^= rng >> 17U;
        rng ^= rng << 5U;
        const int y = static_cast<int>(rng % static_cast<std::uint32_t>(kHeight));
        const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth) +
                                  static_cast<std::size_t>(x);
        if (framebuffer[index] == 0U) {
            framebuffer[index] = (i % 5 == 0) ? 0xCFFFU : 0x7BEFU;
        }
    }

    std::size_t non_black = 0U;
    for (const std::uint16_t pixel : framebuffer) {
        if (pixel != 0U) {
            ++non_black;
        }
    }

    write_ppm(output_path, framebuffer);
    const std::uint64_t hash = fnv1a_pixels(framebuffer);

    std::cout << "{\n"
              << "  \"width\": " << kWidth << ",\n"
              << "  \"height\": " << kHeight << ",\n"
              << "  \"drawn_objects\": " << scene.lastFrameDrawnObjects << ",\n"
              << "  \"drawn_triangles\": " << scene.lastFrameDrawnTriangles << ",\n"
              << "  \"rasterized_triangles\": " << scene.lastFrameRasterizedTriangles << ",\n"
              << "  \"non_black_pixels\": " << non_black << ",\n"
              << "  \"frame_hash\": \"0x" << std::hex << std::setw(16) << std::setfill('0')
              << hash << "\",\n"
              << "  \"output\": \"" << output_path << "\"\n"
              << "}\n";

    delete asteroid;

    if (non_black < 500U || scene.lastFrameRasterizedTriangles <= 0) {
        std::cerr << "Jet smoke render did not produce enough visible geometry\n";
        return 3;
    }

    return 0;
}
