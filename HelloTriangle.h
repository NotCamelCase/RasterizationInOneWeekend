#pragma once

namespace partI
{
    // Frame buffer dimensions
    static const auto g_scWidth = 1280u;
    static const auto g_scHeight = 720u;

    // Transform a given vertex in NDC [-1,1] to raster-space [0, {w|h}]
#define TO_RASTER(v) glm::vec3((g_scWidth * (v.x + 1.0f) / 2), (g_scHeight * (v.y + 1.f) / 2), 1.0f)

    void OutputFrame(const std::vector<glm::vec3>& frameBuffer, const char* filename)
    {
        assert(frameBuffer.size() >= (g_scWidth * g_scHeight));

        FILE* pFile = nullptr;
        fopen_s(&pFile, filename, "w");
        fprintf(pFile, "P3\n%d %d\n%d\n ", g_scWidth, g_scHeight, 255);
        for (auto i = 0; i < g_scWidth * g_scHeight; ++i)
        {
            // Write out color values clamped to [0, 255] 
            uint32_t r = static_cast<uint32_t>(255 * glm::clamp(frameBuffer[i].r, 0.0f, 1.0f));
            uint32_t g = static_cast<uint32_t>(255 * glm::clamp(frameBuffer[i].g, 0.0f, 1.0f));
            uint32_t b = static_cast<uint32_t>(255 * glm::clamp(frameBuffer[i].b, 0.0f, 1.0f));
            fprintf(pFile, "%d %d %d ", r, g, b);
        }
        fclose(pFile);
    }

    void HelloTriangle()
    {
#if 1 // 2DH (x, y, w) coordinates of our triangle's vertices, in counter-clockwise order
        glm::vec3 v0(-0.5, 0.5, 1.0);
        glm::vec3 v1(0.5, 0.5, 1.0);
        glm::vec3 v2(0.0, -0.5, 1.0);
#else // Or optionally go with this set of vertices to see how this triangle would be rasterized
        glm::vec3 v0(-0.5, -0.5, 1.0);
        glm::vec3 v1(-0.5, 0.5, 1.0);
        glm::vec3 v2(0.5, 0.5, 1.0);
#endif

        // Apply viewport transformation
        v0 = TO_RASTER(v0);
        v1 = TO_RASTER(v1);
        v2 = TO_RASTER(v2);

        // Per-vertex color values
        // Notice how each of these RGB colors corresponds to each vertex defined above
        glm::vec3 c0(1, 0, 0);
        glm::vec3 c1(0, 1, 0);
        glm::vec3 c2(0, 0, 1);

        // Base vertex matrix
        glm::mat3 M = // transpose(glm::mat3(v0, v1, v2));
        {
            // Notice that glm is itself column-major)
            { v0.x, v1.x, v2.x},
            { v0.y, v1.y, v2.y},
            { v0.z, v1.z, v2.z},
        };

        // Compute the inverse of vertex matrix to use it for setting up edge functions
        M = inverse(M);

        // Calculate edge functions based on the vertex matrix once
        glm::vec3 E0 = M * glm::vec3(1, 0, 0); // == M[0]
        glm::vec3 E1 = M * glm::vec3(0, 1, 0); // == M[1]
        glm::vec3 E2 = M * glm::vec3(0, 0, 1); // == M[2]

        // Allocate and clear the frame buffer before starting to render to it
        std::vector<glm::vec3> frameBuffer(g_scWidth * g_scHeight, glm::vec3(0, 0, 0)); // clear color black = vec3(0, 0, 0)

        // Start rasterizing by looping over pixels to output a per-pixel color
        for (auto y = 0; y < g_scHeight; y++)
        {
            for (auto x = 0; x < g_scWidth; x++)
            {
                // Sample location at the center of each pixel
                glm::vec3 sample = { x + 0.5f, y + 0.5f, 1.0f };

                // Evaluate edge functions at every fragment
                float alpha = glm::dot(E0, sample);
                float beta = glm::dot(E1, sample);
                float gamma = glm::dot(E2, sample);

                // If sample is "inside" of all three half-spaces bounded by the three edges of our triangle, it's 'on' the triangle
                if ((alpha >= 0.0f) && (beta >= 0.0f) && (gamma >= 0.0f))
                {
                    assert(((alpha + beta + gamma) - 1.0f) <= glm::epsilon<float>());

#if 1 // Blend per-vertex colors defined above using the coefficients from edge functions
                    frameBuffer[x + y * g_scWidth] = glm::vec3(c0 * alpha + c1 * beta + c2 * gamma);
#else // Or go with flat color if that's your thing
                    frameBuffer[x + y * g_scWidth] = glm::vec3(1, 0, 0);
#endif
                }
            }
        }

        // Rendering of one frame is finished, output a .PPM file of the contents of our frame buffer to see what we actually just rendered
        OutputFrame(frameBuffer, "../render_hello_triangle.ppm");
    }
}