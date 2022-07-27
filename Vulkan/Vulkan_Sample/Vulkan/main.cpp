
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
#include "sample/subpasses/subpasses.h"
#include "sample/offscreen/offscreen.h"
#include "sample/particlefire/particlefire.h"
#include "sample/stencilbuffer/stencilbuffer.h"
#include "sample/separatevertexattributes/separatevertexattributes.h"
#include "sample/gltfloading/gltfloading.h"
#include "sample/gltfskinning/gltfskinning.h"
#include "sample/gltfscenerendering/gltfscenerendering.h"

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
//    InputAttachments app("inputattachments");
//    SubPasses app("subpasses");
//    OffScreen app("offscreen");
//    ParticleFire app("particlefire");
//    StencilBuffer app("stencilbuffer");
//    SeparateVertexAttributes app("separatevertexattributes");
//    GltfLoading app("gltfloading");
//    GltfSkinning app("gltfskinning");
    GltfSceneRendering app("gltfscenerendering");
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
