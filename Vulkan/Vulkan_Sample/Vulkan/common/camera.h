
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

class Camera
{
public:
    float getNearClip(){return m_znear;}
    float getFarClip(){return m_zfar;}
    void setRotationSpeed(float rotationSpeed) {m_rotationSpeed = rotationSpeed;}
    void setMovementSpeed(float movementSpeed) {m_movementSpeed = movementSpeed;}
    bool isMoving();

    void setPosition(glm::vec3 position);
    void setRotation(glm::vec3 rotation);
    void rotate(glm::vec3 delta);
    void setTranslation(glm::vec3 translation);
    void translate(glm::vec3 delta);
    
    void updateViewMatrix();
    void updateAspectRatio(float aspect);
    void setPerspective(float fov, float aspect, float znear, float zfar);
    void update(float deltaTime);
    bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime);

public:
    float m_rotationSpeed = 1.0f;
    float m_movementSpeed = 1.0f;
    
    bool m_isNeedUpdated = false;
    bool m_isFlipY = false;
    
    int m_moveAxis = 0; // 0.不移动, [1-6] x-,x+,y-,y+,z-,z+
    
    glm::mat4 m_viewMat;
    glm::mat4 m_projMat;
    
private:
    float m_fov;
    float m_znear, m_zfar;
    glm::vec3 m_rotation = glm::vec3();
    glm::vec3 m_position = glm::vec3();
    glm::vec4 m_viewPos = glm::vec4();
};
