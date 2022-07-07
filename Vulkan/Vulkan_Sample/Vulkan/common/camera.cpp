
#include "camera.h"

bool Camera::isMoving()
{
    return m_isMoveLeft || m_isMoveRight || m_isMoveUp || m_isMoveDown;
}

void Camera::setPosition(glm::vec3 position)
{
    m_position = position;
    updateViewMatrix();
}

void Camera::setRotation(glm::vec3 rotation)
{
    m_rotation = rotation;
    updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta)
{
    m_rotation += delta;
    updateViewMatrix();
}

void Camera::setTranslation(glm::vec3 translation)
{
    m_position = translation;
    updateViewMatrix();
}

void Camera::translate(glm::vec3 delta)
{
    m_position += delta;
    updateViewMatrix();
}

void Camera::updateViewMatrix()
{
    glm::mat4 rotM = glm::mat4(1.0f);
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(m_rotation.x * (m_isFlipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 translation = m_position;
    if (m_isFlipY) {
        translation.y *= -1.0f;
    }
    transM = glm::translate(glm::mat4(1.0f), translation);

    m_viewMat = rotM * transM;

    m_viewPos = glm::vec4(m_position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
    m_isNeedUpdated = true;
}

void Camera::updateAspectRatio(float aspect)
{
    m_projMat = glm::perspective(glm::radians(m_fov), aspect, m_znear, m_zfar);
    if (m_isFlipY) {
        m_projMat[1][1] *= -1.0f;
    }
}

void Camera::setPerspective(float fov, float aspect, float znear, float zfar)
{
    m_fov = fov;
    m_znear = znear;
    m_zfar = zfar;
    m_projMat = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    if (m_isFlipY) {
        m_projMat[1][1] *= -1.0f;
    }
}

void Camera::update(float deltaTime)
{
    m_isNeedUpdated = false;
    if (isMoving())
    {
        glm::vec3 camFront;
        camFront.x = -cos(glm::radians(m_rotation.x)) * sin(glm::radians(m_rotation.y));
        camFront.y = sin(glm::radians(m_rotation.x));
        camFront.z = cos(glm::radians(m_rotation.x)) * cos(glm::radians(m_rotation.y));
        camFront = glm::normalize(camFront);

        float moveSpeed = deltaTime * m_movementSpeed;

        if (m_isMoveUp)
            m_position += camFront * moveSpeed;
        if (m_isMoveDown)
            m_position -= camFront * moveSpeed;
        if (m_isMoveLeft)
            m_position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        if (m_isMoveRight)
            m_position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

//        std::cout << m_position.x <<","<< m_position.y <<","<< m_position.z << std::endl;
        
        updateViewMatrix();
    }
}

bool Camera::updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
{
    bool retVal = false;

    const float deadZone = 0.0015f;
    const float range = 1.0f - deadZone;

    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(m_rotation.x)) * sin(glm::radians(m_rotation.y));
    camFront.y = sin(glm::radians(m_rotation.x));
    camFront.z = cos(glm::radians(m_rotation.x)) * cos(glm::radians(m_rotation.y));
    camFront = glm::normalize(camFront);

    float moveSpeed = deltaTime * m_movementSpeed * 2.0f;
    float rotSpeed = deltaTime * m_rotationSpeed * 50.0f;
     
    // Move
    if (fabsf(axisLeft.y) > deadZone)
    {
        float pos = (fabsf(axisLeft.y) - deadZone) / range;
        m_position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
        retVal = true;
    }
    if (fabsf(axisLeft.x) > deadZone)
    {
        float pos = (fabsf(axisLeft.x) - deadZone) / range;
        m_position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
        retVal = true;
    }

    // Rotate
    if (fabsf(axisRight.x) > deadZone)
    {
        float pos = (fabsf(axisRight.x) - deadZone) / range;
        m_rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
        retVal = true;
    }
    if (fabsf(axisRight.y) > deadZone)
    {
        float pos = (fabsf(axisRight.y) - deadZone) / range;
        m_rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
        retVal = true;
    }

    if (retVal)
    {
        updateViewMatrix();
    }

    return retVal;
}
