
#include "noise.h"
#include <stdlib.h>
#include <random>

PerlinNoise::PerlinNoise()
{
    std::vector<uint8_t> lookup;
    lookup.resize(256);
    std::iota(lookup.begin(), lookup.end(), 0);
    std::default_random_engine rndEngine(std::random_device{}());
    std::shuffle(lookup.begin(), lookup.end(), rndEngine);

    for (uint32_t i = 0; i < 256; i++)
    {
        m_permutations[i] = m_permutations[256 + i] = lookup[i];
    }
}

float PerlinNoise::fade(float t)
{
    //使用5次方插值
    return t*t*t*(t*(t*6.0f - 15.0f) + 10.0f);
}

float PerlinNoise::lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float y, float z)
{
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float PerlinNoise::noise(float x, float y, float z)
{
    // Find unit cube that contains point
    int32_t X = (int32_t)floor(x) & 255;
    int32_t Y = (int32_t)floor(y) & 255;
    int32_t Z = (int32_t)floor(z) & 255;
    // Find relative x,y,z of point in cube
    x -= floor(x);
    y -= floor(y);
    z -= floor(z);

    // Compute fade curves for each of x,y,z
    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    // Hash coordinates of the 8 cube corners
    uint32_t A = m_permutations[X] + Y;
    uint32_t AA = m_permutations[A] + Z;
    uint32_t AB = m_permutations[A + 1] + Z;
    uint32_t B = m_permutations[X + 1] + Y;
    uint32_t BA = m_permutations[B] + Z;
    uint32_t BB = m_permutations[B + 1] + Z;

    // And add blended results for 8 corners of the cube;    
    float res = lerp(w, lerp(v,
        lerp(u, grad(m_permutations[AA], x, y, z), grad(m_permutations[BA], x - 1, y, z)), lerp(u, grad(m_permutations[AB], x, y - 1, z), grad(m_permutations[BB], x - 1, y - 1, z))),
        lerp(v, lerp(u, grad(m_permutations[AA + 1], x, y, z - 1), grad(m_permutations[BA + 1], x - 1, y, z - 1)), lerp(u, grad(m_permutations[AB + 1], x, y - 1, z - 1), grad(m_permutations[BB + 1], x - 1, y - 1, z - 1))));
    return res;
}


FractalNoise::FractalNoise(const PerlinNoise& perlinNoise)
{
    m_perlinNoise = perlinNoise;
    m_octaves = 6;
    m_persistence = 0.5f;
}

float FractalNoise::noise(float x, float y, float z)
{
    float sum = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float total = 0.0f;
    for (uint32_t i = 0; i < m_octaves; i++)
    {
        sum += m_perlinNoise.noise(x * frequency, y * frequency, z * frequency) * amplitude;
        total += amplitude;
        amplitude *= m_persistence;
        frequency *= 2.0;
    }

    sum = sum / total;
    return (sum + 1.0f)/2.0;
}


Noise::Noise()
{
}

Noise::~Noise()
{}

VkDescriptorImageInfo Noise::getDescriptorImageInfo()
{
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = m_imageLayout;
    imageInfo.imageView = m_imageView;
    imageInfo.sampler = m_sampler;
    return imageInfo;
}

void Noise::clear()
{
    vkDestroySampler(Tools::m_device, m_sampler, nullptr);
    vkDestroyImageView(Tools::m_device, m_imageView, nullptr);
    vkDestroyImage(Tools::m_device, m_image, nullptr);
    vkFreeMemory(Tools::m_device, m_imageMemory, nullptr);
}

Noise* Noise::createNoise3D(uint32_t width, uint32_t height, uint32_t depth, VkQueue transferQueue)
{
    Noise* noise = new Noise();
    noise->m_width = width;
    noise->m_height = height;
    noise->m_depth = depth;
    noise->m_mipLevels = 1;
    noise->m_layerCount = 1;
    noise->m_fromat = VK_FORMAT_R8_UNORM;
    noise->m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 3D texture support in Vulkan is mandatory (in contrast to OpenGL) so no need to check if it's supported
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties( Tools::m_physicalDevice, noise->m_fromat, &formatProperties);
    // Check if format supports transfer
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
    {
        throw std::runtime_error("device does not support flag TRANSFER_DST for selected texture format!");
    }
    // Check if GPU supports requested 3D texture dimensions
    uint32_t maxImageDimension3D( Tools::m_deviceProperties.limits.maxImageDimension3D);
    if (width > maxImageDimension3D || height > maxImageDimension3D || depth > maxImageDimension3D)
    {
        throw std::runtime_error("requested texture dimensions is greater than supported 3D texture dimension!");
    }
    
    const uint32_t totalSize = noise->m_width * noise->m_height * noise->m_depth;
    uint8_t *data = new uint8_t[totalSize];
    memset(data, 0, totalSize);
    std::cout << "Generating " << noise->m_width << " x " << noise->m_height << " x " << noise->m_depth << " noise texture..." << std::endl;
    auto tStart = std::chrono::high_resolution_clock::now();
    PerlinNoise perlinNoise;
    FractalNoise fractalNoise(perlinNoise);
    const float noiseScale = static_cast<float>(rand() % 10) + 4.0f;

    for (int32_t z = 0; z < noise->m_depth; z++)
    {
        for (int32_t y = 0; y < noise->m_height; y++)
        {
            for (int32_t x = 0; x < noise->m_width; x++)
            {
                float nx = (float)x / (float)noise->m_width;
                float ny = (float)y / (float)noise->m_height;
                float nz = (float)z / (float)noise->m_depth;

                float n = fractalNoise.noise(nx * noiseScale, ny * noiseScale, nz * noiseScale);
//                float n = 20.0 * perlinNoise.noise(nx, ny, nz);
                n = n - floor(n);
                uint32_t index = x + y * noise->m_width + z * noise->m_width * noise->m_height;
                data[index] = static_cast<uint8_t>(floor(n * 255));
            }
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Done in " << tDiff << "ms" << std::endl;
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    Tools::createBufferAndMemoryThenBind(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);
        
    Tools::mapMemory(stagingMemory, totalSize, data);
    
    Tools::createImageAndMemoryThenBind(noise->m_fromat, noise->m_width, noise->m_height, noise->m_mipLevels, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        noise->m_image, noise->m_imageMemory, 0, noise->m_depth, VK_IMAGE_TYPE_3D);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    
    Tools::setImageLayout(cmd, noise->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
    
    VkBufferImageCopy bufferCopyRegion{};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = noise->m_width;
    bufferCopyRegion.imageExtent.height = noise->m_height;
    bufferCopyRegion.imageExtent.depth = noise->m_depth;
    
    vkCmdCopyBufferToImage(cmd, stagingBuffer, noise->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
    
    Tools::setImageLayout(cmd, noise->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          noise->m_imageLayout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
    
    Tools::flushCommandBuffer(cmd, transferQueue, true);
    
    delete[] data;
    vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);

    Tools::createImageView(noise->m_image, noise->m_fromat, VK_IMAGE_ASPECT_COLOR_BIT, noise->m_mipLevels, 1, noise->m_imageView, VK_IMAGE_VIEW_TYPE_3D);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, noise->m_mipLevels, noise->m_sampler);
    return noise;
}
