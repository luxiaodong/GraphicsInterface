
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

#define PARTICLE_COUNT 512

struct Particle {
    glm::vec4 pos;
    glm::vec4 color;
    float alpha;
    float size;
    float rotation;
    uint32_t type;
    // Attributes not used in shader
    glm::vec4 vel;
    float rotationSpeed;
};

class ParticleFire : public Application
{
public:    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    };
    
    struct ParticleUniform {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec2 viewportDim;
        float pointSize;
    };
    
    ParticleFire(std::string title);
    virtual ~ParticleFire();
    
    virtual void init();
    virtual void initCamera();
    virtual void clear();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    
protected:
    void prepareParticle();
    void initParticle(Particle* particle, glm::vec3 emitterPos = glm::vec3(0.0f, -6.0f, 0.0f));
    void transitionParticle(Particle *particle);
    void updateParticles();
    
protected:
    VkPipeline m_particlePipeline;
    VkDescriptorSet m_particleDescriptorSet;
    VkBuffer m_particleUniformBuffer;
    VkDeviceMemory m_particleUniformMemory;
    
    std::vector<Particle> m_particles;
    glm::vec3 m_minVel = glm::vec3(-3.0f, 0.5f, -3.0f);
    glm::vec3 m_maxVel = glm::vec3(3.0f, 7.0f, 3.0f);
    VkBuffer m_particleBuffer;
    VkDeviceMemory m_particleMemory;
    Texture* m_pFire;
    Texture* m_pSmoke;

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;

private:
    GltfLoader m_environmentLoader;
    Texture* m_pFloorColor;
    Texture* m_pFloorNormal;
};
