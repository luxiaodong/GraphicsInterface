
#include "sample/triangle/triangle.h"
#include "sample/pipelines/pipelines.h"
#include "sample/descriptorsets/descriptorsets.h"
#include "sample/dynamicuniformbuffer/dynamicuniformbuffer.h"
#include "sample/pushconstants/pushconstants.h"
#include "sample/specializationconstants/specializationconstants.h"
#include "sample/texturemapping/texturemapping.h"
#include "sample/texturearray/texturearray.h"
#include "sample/texturecubemapping/texturecubemapping.h"
#include "sample/texturecubemaparray/texturecubemaparray.h"
#include "sample/texture3d/texture3d.h"
#include "sample/inputattachments/inputattachments.h"

int main(int argc, const char * argv[])
{
//    Triangle app("triangle");
//    Pipelines app("pipeline");
//    Descriptorsets app("descriptorsets");
//    DynamicUniformBuffer app("dynamicuniformbuffer");
//    PushConstants app("pushconstants");
//    SpecializationConstants app("specializationconstants");
//    TextureMapping app("texturemapping");
//    TextureArray app("texturearray");
//    TextureCubeMapping app("texturecubemapping");
//    TextureCubemapArray app("texturecubemaparray");
//    Texture3Dim app("texture3d");
    
    InputAttachments app("inputattachments");
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
