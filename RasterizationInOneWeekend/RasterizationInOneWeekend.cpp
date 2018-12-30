#include "pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "../HelloTriangle.h"
#include "../Go3D.h"
#include "../GoWild.h"

int main()
{
    // Part I: Hello, Triangle!
    partI::HelloTriangle();

    // Part II: Go 3D!
    partII::Go3D();

    // Part III: Go Wild!
    partIII::GoWild();
}