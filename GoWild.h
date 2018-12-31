#pragma once

namespace partIII
{
    // Frame buffer dimensions
    static const auto g_scWidth = 1280u;
    static const auto g_scHeight = 720u;

// Silence macro redefinition warnings
#undef TO_RASTER
// Transform a given vertex in clip-space [-w,w] to raster-space [0, {w|h}]
#define TO_RASTER(v) glm::vec4((g_scWidth * (v.x + v.w) / 2), (g_scHeight * (v.w - v.y) / 2), v.z, v.w)

    // Used for texture mapping
    struct Texture
    {
        stbi_uc* m_Data = nullptr;
        int32_t m_Width = -1;
        int32_t m_Height = -1;
        int32_t m_NumChannels = -1;
    };

    // Vertex data to be fed into each VS invocation as input
    struct VertexInput
    {
        glm::vec3 Pos;
        glm::vec3 Normal;
        glm::vec2 TexCoords;
    };

    // Vertex Shader payload, which will be passed to each FS invocation as input
    struct FragmentInput
    {
        glm::vec3 Normal;
        glm::vec2 TexCoords;
    };

    // Indexed mesh
    struct Mesh
    {
        // Offset into the global index buffer
        uint32_t m_IdxOffset = 0u;

        // How many indices this mesh contains. Number of triangles therefore equals (m_IdxCount / 3)
        uint32_t m_IdxCount = 0u;

        // Texture map from material
        std::string m_DiffuseTexName;
    };

    void DrawIndexed(std::vector<glm::vec3>& frameBuffer, std::vector<float>& depthBuffer, std::vector<VertexInput>& vertexBuffer, std::vector<uint32_t>& indexBuffer, Mesh& mesh, glm::mat4& MVP, Texture* pTexture);

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

    void InitializeSceneObjects(const char* fileName, std::vector<Mesh>& meshBuffer, std::vector<VertexInput>& vertexBuffer, std::vector<uint32_t>& indexBuffer, std::map<std::string, Texture*>& textures)
    {
        tinyobj::attrib_t attribs;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err = "";

        bool ret = tinyobj::LoadObj(&attribs, &shapes, &materials, &err, fileName, "../assets/", true /*triangulate*/);
        if (ret)
        {
            // Process materials to load images
            {
                for (unsigned i = 0; i < materials.size(); i++)
                {
                    const tinyobj::material_t& m = materials[i];

                    std::string diffuseTexName = m.diffuse_texname;
                    assert(!diffuseTexName.empty() && "Mesh missing texture!");

                    if (textures.find(diffuseTexName) == textures.end())
                    {
                        Texture* pAlbedo = new Texture();

                        pAlbedo->m_Data = stbi_load(("../assets/" + diffuseTexName).c_str(), &pAlbedo->m_Width, &pAlbedo->m_Height, &pAlbedo->m_NumChannels, 0);
                        assert(pAlbedo->m_Data != nullptr && "Failed to load image!");

                        textures[diffuseTexName] = pAlbedo;
                    }
                }
            }

            // Process vertices
            {
                // POD of indices of vertex data provided by tinyobjloader, used to map unique vertex data to indexed primitive
                struct IndexedPrimitive
                {
                    uint32_t PosIdx;
                    uint32_t NormalIdx;
                    uint32_t UVIdx;

                    bool operator<(const IndexedPrimitive& other) const
                    {
                        return memcmp(this, &other, sizeof(IndexedPrimitive)) > 0;
                    }
                };

                std::map<IndexedPrimitive, uint32_t> indexedPrims;
                for (size_t s = 0; s < shapes.size(); s++)
                {
                    const tinyobj::shape_t& shape = shapes[s];

                    uint32_t meshIdxBase = indexBuffer.size();
                    for (size_t i = 0; i < shape.mesh.indices.size(); i++)
                    {
                        auto index = shape.mesh.indices[i];

                        // Fetch indices to construct an IndexedPrimitive to first look up existing unique vertices
                        int vtxIdx = index.vertex_index;
                        assert(vtxIdx != -1);

                        bool hasNormals = index.normal_index != -1;
                        bool hasUV = index.texcoord_index != -1;

                        int normalIdx = index.normal_index;
                        int uvIdx = index.texcoord_index;

                        IndexedPrimitive prim;
                        prim.PosIdx = vtxIdx;
                        prim.NormalIdx = hasNormals ? normalIdx : UINT32_MAX;
                        prim.UVIdx = hasUV ? uvIdx : UINT32_MAX;

                        auto res = indexedPrims.find(prim);
                        if (res != indexedPrims.end())
                        {
                            // Vertex is already defined in terms of POS/NORMAL/UV indices, just append index data to index buffer
                            indexBuffer.push_back(res->second);
                        }
                        else
                        {
                            // New unique vertex found, get vertex data and append it to vertex buffer and update indexed primitives
                            auto newIdx = vertexBuffer.size();
                            indexedPrims[prim] = newIdx;
                            indexBuffer.push_back(newIdx);

                            auto vx = attribs.vertices[3 * index.vertex_index];
                            auto vy = attribs.vertices[3 * index.vertex_index + 1];
                            auto vz = attribs.vertices[3 * index.vertex_index + 2];

                            glm::vec3 pos(vx, vy, vz);

                            glm::vec3 normal(0.f);
                            if (hasNormals)
                            {
                                auto nx = attribs.normals[3 * index.normal_index];
                                auto ny = attribs.normals[3 * index.normal_index + 1];
                                auto nz = attribs.normals[3 * index.normal_index + 2];

                                normal.x = nx;
                                normal.y = ny;
                                normal.z = nz;
                            }

                            glm::vec2 uv(0.f);
                            if (hasUV)
                            {
                                auto ux = attribs.texcoords[2 * index.texcoord_index];
                                auto uy = 1.f - attribs.texcoords[2 * index.texcoord_index + 1];

                                uv.s = glm::abs(ux);
                                uv.t = glm::abs(uy);
                            }

                            VertexInput uniqueVertex = { pos, normal, uv };
                            vertexBuffer.push_back(uniqueVertex);
                        }
                    }

                    // Push new mesh to be rendered in the scene 
                    Mesh mesh;
                    mesh.m_IdxOffset = meshIdxBase;
                    mesh.m_IdxCount = shape.mesh.indices.size();

                    assert((shape.mesh.material_ids[0] != -1) && "Mesh missing a material!");
                    mesh.m_DiffuseTexName = materials[shape.mesh.material_ids[0]].diffuse_texname; // No per-face material but fixed one

                    meshBuffer.push_back(mesh);
                }
            }
        }
        else
        {
            printf("ERROR: %s\n", err.c_str());
            assert(false && "Failed to load .OBJ file, check file paths!");
        }
    }

    void GoWild()
    {
        // Allocate and clear the frame buffer before starting to render to it
        std::vector<glm::vec3> frameBuffer(g_scWidth * g_scHeight, glm::vec3(0, 0, 0)); // clear color black = vec3(0, 0, 0)

        // Allocate and clear the depth buffer ro FLT_MAX as we utilize z values to resolve visibility now
        std::vector<float> depthBuffer(g_scWidth * g_scHeight, FLT_MAX);

        // We will have single giant index and vertex buffer to draw indexed meshes
        std::vector<VertexInput> vertexBuffer;
        std::vector<uint32_t> indexBuffer;

        // Store data of all scene objects to be drawn
        std::vector<Mesh> primitives;

        // All texture maps loaded. Every mesh will reference their texture map by name at draw time
        std::map<std::string, Texture*> textures;

        const auto fileName = "../assets/sponza.obj";

        // Load .OBJ file and process it to construct a scene of multiple meshes
        InitializeSceneObjects(fileName, primitives, vertexBuffer, indexBuffer, textures);

        // Build view & projection matrices (right-handed sysem)
        float nearPlane = 0.125f;
        float farPlane = 5000.f;
        glm::vec3 eye(0, -8.5, -5);
        glm::vec3 lookat(20, 5, 1);
        glm::vec3 up(0, 1, 0);

        glm::mat4 view = glm::lookAt(eye, lookat, up);
        view = glm::rotate(view, glm::radians(-30.f), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.f), static_cast<float>(g_scWidth) / static_cast<float>(g_scHeight), nearPlane, farPlane);

        glm::mat4 MVP = proj * view;

        // Loop over all objects in the scene and draw them one by one
        for (auto i = 0; i < primitives.size(); i++)
        {
            DrawIndexed(frameBuffer, depthBuffer, vertexBuffer, indexBuffer, primitives[i], MVP, textures[primitives[i].m_DiffuseTexName]);
        }

        // Rendering of one frame is finished, output a .PPM file of the contents of our frame buffer to see what we actually just rendered
        OutputFrame(frameBuffer, "../render_go_wild.ppm");

        // Clean up resources
        for (const auto& elem : textures)
            delete elem.second;
    }

    // Vertex Shader to apply perspective projections and also pass vertex attributes to Fragment Shader
    glm::vec4 VS(const VertexInput& input, const glm::mat4& MVP, FragmentInput& output)
    {
        // Simply pass normal and texture coordinates directly to FS
        output.Normal = input.Normal;
        output.TexCoords = input.TexCoords;

        // Output a clip-space vec4 that will be used to rasterize parent triangle
        return (MVP * glm::vec4(input.Pos, 1.0f));
    }

    // Fragment Shader that will be run at every visible pixel on triangles to shade fragments
    glm::vec3 FS(const FragmentInput& input, Texture* pTexture)
    {
#if 1 // Render textured polygons

        // By using fractional part of texture coordinates only, we will REPEAT (or WRAP) the same texture multiple times
        uint32_t idxS = static_cast<uint32_t>((input.TexCoords.s - static_cast<int64_t>(input.TexCoords.s)) * pTexture->m_Width - 0.5f);
        uint32_t idxT = static_cast<uint32_t>((input.TexCoords.t - static_cast<int64_t>(input.TexCoords.t)) * pTexture->m_Height - 0.5f);
        uint32_t idx = (idxT * pTexture->m_Width + idxS) * pTexture->m_NumChannels;

        float r = static_cast<float>(pTexture->m_Data[idx++] * (1.f / 255));
        float g = static_cast<float>(pTexture->m_Data[idx++] * (1.f / 255));
        float b = static_cast<float>(pTexture->m_Data[idx++] * (1.f / 255));

        return glm::vec3(r, g, b);
#else // Render interpolated normals
        return (input.Normals) * glm::vec3(0.5) + glm::vec3(0.5); // transform normal values [-1, 1] -> [0, 1] to visualize better
#endif
    }

    bool EvaluateEdgeFunction(const glm::vec3& E, const glm::vec2& sample)
    {
        // Interpolate edge function at given sample
        float result = (E.x * sample.x) + (E.y * sample.y) + E.z;

        // Apply tie-breaking rules on shared vertices in order to avoid double-shading fragments
        if (result > 0.0f) return true;
        else if (result < 0.0f) return false;

        if (E.x > 0.f) return true;
        else if (E.x < 0.0f) return false;

        if ((E.x == 0.0f) && (E.y < 0.0f)) return false;
        else return true;
    }

    void DrawIndexed(std::vector<glm::vec3>& frameBuffer, std::vector<float>& depthBuffer, std::vector<VertexInput>& vertexBuffer, std::vector<uint32_t>& indexBuffer, Mesh& mesh, glm::mat4& MVP, Texture* pTexture)
    {
        assert(pTexture != nullptr);

        const int32_t triCount = mesh.m_IdxCount / 3;

        // Loop over triangles in a given mesh and rasterize them
        for (int32_t idx = 0; idx < triCount; idx++)
        {
            // Fetch vertex input of next triangle to be rasterized
            const VertexInput& vi0 = vertexBuffer[indexBuffer[mesh.m_IdxOffset + (idx * 3)]];
            const VertexInput& vi1 = vertexBuffer[indexBuffer[mesh.m_IdxOffset + (idx * 3 + 1)]];
            const VertexInput& vi2 = vertexBuffer[indexBuffer[mesh.m_IdxOffset + (idx * 3 + 2)]];

            // To collect VS payload
            FragmentInput fi0;
            FragmentInput fi1;
            FragmentInput fi2;

            // Invoke VS for each vertex of the triangle to transform them from object-space to clip-space (-w, w)
            glm::vec4 v0Clip = VS(vi0, MVP, fi0);
            glm::vec4 v1Clip = VS(vi1, MVP, fi1);
            glm::vec4 v2Clip = VS(vi2, MVP, fi2);

            // Apply viewport transformation
            // Notice that we haven't applied homogeneous division and are still utilizing homogeneous coordinates
            glm::vec4 v0Homogen = TO_RASTER(v0Clip);
            glm::vec4 v1Homogen = TO_RASTER(v1Clip);
            glm::vec4 v2Homogen = TO_RASTER(v2Clip);

            // Base vertex matrix
            glm::mat3 M =
            {
                // Notice that glm is itself column-major)
                { v0Homogen.x, v1Homogen.x, v2Homogen.x},
                { v0Homogen.y, v1Homogen.y, v2Homogen.y},
                { v0Homogen.w, v1Homogen.w, v2Homogen.w},
            };

            // Singular vertex matrix (det(M) == 0.0) means that the triangle has zero area,
            // which in turn means that it's a degenerate triangle which should not be rendered anyways,
            // whereas (det(M) > 0) implies a back-facing triangle so we're going to skip such primitives
            float det = glm::determinant(M);
            if (det >= 0.0f)
                continue;

            // Compute the inverse of vertex matrix to use it for setting up edge & constant functions
            M = inverse(M);

            // Set up edge functions based on the vertex matrix
            // We also apply some scaling to edge functions to be more robust.
            // This is fine, as we are working with homogeneous coordinates and do not disturb the sign of these functions.
            glm::vec3 E0 = M[0] / (glm::abs(M[0].x) + glm::abs(M[0].y));
            glm::vec3 E1 = M[1] / (glm::abs(M[1].x) + glm::abs(M[1].y));
            glm::vec3 E2 = M[2] / (glm::abs(M[2].x) + glm::abs(M[2].y));

            // Calculate constant function to interpolate 1/w
            glm::vec3 C = M * glm::vec3(1, 1, 1);

            // Calculate z interpolation vector
            glm::vec3 Z = M * glm::vec3(v0Clip.z, v1Clip.z, v2Clip.z);

            // Calculate normal interpolation vector
            glm::vec3 PNX = M * glm::vec3(fi0.Normal.x, fi1.Normal.x, fi2.Normal.x);
            glm::vec3 PNY = M * glm::vec3(fi0.Normal.y, fi1.Normal.y, fi2.Normal.y);
            glm::vec3 PNZ = M * glm::vec3(fi0.Normal.z, fi1.Normal.z, fi2.Normal.z);

            // Calculate UV interpolation vector
            glm::vec3 PUVS = M * glm::vec3(fi0.TexCoords.s, fi1.TexCoords.s, fi2.TexCoords.s);
            glm::vec3 PUVT = M * glm::vec3(fi0.TexCoords.t, fi1.TexCoords.t, fi2.TexCoords.t);

            // Start rasterizing by looping over pixels to output a per-pixel color
            for (auto y = 0; y < g_scHeight; y++)
            {
                for (auto x = 0; x < g_scWidth; x++)
                {
                    // Sample location at the center of each pixel
                    glm::vec2 sample = { x + 0.5f, y + 0.5f };

                    // Evaluate edge functions at current fragment
                    bool inside0 = EvaluateEdgeFunction(E0, sample);
                    bool inside1 = EvaluateEdgeFunction(E1, sample);
                    bool inside2 = EvaluateEdgeFunction(E2, sample);

                    // If sample is "inside" of all three half-spaces bounded by the three edges of the triangle, it's 'on' the triangle
                    if (inside0 && inside1 && inside2)
                    {
                        // Interpolate 1/w at current fragment
                        float oneOverW = (C.x * sample.x) + (C.y * sample.y) + C.z;

                        // w = 1/(1/w)
                        float w = 1.f / oneOverW;

                        // Interpolate z that will be used for depth test
                        float zOverW = (Z.x * sample.x) + (Z.y * sample.y) + Z.z;
                        float z = zOverW * w;

                        if (z <= depthBuffer[x + y * g_scWidth])
                        {
                            // Depth test passed; update depth buffer value
                            depthBuffer[x + y * g_scWidth] = z;

                            // Interpolate normal
                            float nxOverW = (PNX.x * sample.x) + (PNX.y * sample.y) + PNX.z;
                            float nyOverW = (PNY.x * sample.x) + (PNY.y * sample.y) + PNY.z;
                            float nzOverW = (PNZ.x * sample.x) + (PNZ.y * sample.y) + PNZ.z;

                            // Interpolate texture coordinates
                            float uOverW = (PUVS.x * sample.x) + (PUVS.y * sample.y) + PUVS.z;
                            float vOverW = (PUVT.x * sample.x) + (PUVT.y * sample.y) + PUVT.z;

                            // Final vertex attributes to be passed to FS
                            glm::vec3 normal = glm::vec3(nxOverW, nyOverW, nzOverW) * w; // {nx/w, ny/w, nz/w} * w -> {nx, ny, nz}
                            glm::vec2 texCoords = glm::vec2(uOverW, vOverW) * w; // {u/w, v/w} * w -> {u, v}

                            // Pass interpolated normal & texture coordinates to FS
                            FragmentInput fsInput = { normal, texCoords };

                            // Invoke fragment shader to output a color for each fragment
                            glm::vec3 outputColor = FS(fsInput, pTexture);

                            // Write new color at this fragment
                            frameBuffer[x + y * g_scWidth] = outputColor;
                        }
                    }
                }
            }
        }
    }
}