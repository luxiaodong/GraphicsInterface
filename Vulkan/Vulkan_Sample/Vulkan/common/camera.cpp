
#include "camera.h"

bool Camera::isMoving()
{
    return m_moveAxis;
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

    if (m_isFirstPersion)
    {
        m_viewMat = rotM * transM;
    }
    else
    {
        m_viewMat = transM * rotM;
    }

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
        float moveSpeed = 5.0f*deltaTime * m_movementSpeed;
        
        switch (m_moveAxis) {
            case 1:
                m_position.x -= moveSpeed;
                break;
            case 2:
                m_position.x += moveSpeed;
                break;
            case 3:
                m_position.y -= moveSpeed;
                break;
            case 4:
                m_position.y += moveSpeed;
                break;
            case 5:
                m_position.z -= moveSpeed;
                break;
            case 6:
                m_position.z += moveSpeed;
                break;
                
            default:
                break;
        }
        
//        std::cout << moveSpeed << std::endl;
//        std::cout << m_position.x <<","<< m_position.y <<","<< m_position.z << std::endl;
        
        updateViewMatrix();
        m_moveAxis = 0;
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


void Camera::lookAt(glm::vec3 position, glm::vec3 target, glm::vec3 up)
{
    m_position = position;
    m_viewMat = makeLookAtMatrix(position, target, up);
}

void Camera::projection(float fovy, float aspect, float znear, float zfar)
{
    m_projMat = makePerspectiveMatrix(glm::radians(fovy), aspect, znear, zfar);
    if (m_isFlipY) {
        m_projMat[1][1] *= -1.0f;
    }
}

glm::mat4 Camera::makeLookAtMatrix(glm::vec3& eye_position, glm::vec3& target_position, glm::vec3& up_dir)
{
    glm::vec3 up = glm::normalize(up_dir);
    glm::vec3 f = glm::normalize(target_position - eye_position);
    glm::vec3 s = glm::normalize(glm::cross(f, up));
    glm::vec3 u = glm::normalize(glm::cross(s, f));
    
    glm::mat4 view_mat = glm::mat4(1.0f);
    
    view_mat[0][0] = s.x;
    view_mat[1][0] = s.y;
    view_mat[2][0] = s.z;
    view_mat[3][0] = -glm::dot(s, eye_position);
    view_mat[0][1] = u.x;
    view_mat[1][1] = u.y;
    view_mat[2][1] = u.z;
    view_mat[3][1] = -glm::dot(u, eye_position);
    view_mat[0][2] = -f.x;
    view_mat[1][2] = -f.y;
    view_mat[2][2] = -f.z;
    view_mat[3][2] = glm::dot(f, eye_position);
    return view_mat;
}

glm::mat4 Camera::makePerspectiveMatrix(float fovy, float aspect, float znear, float zfar)
{
    float tan_half_fovy = tan(fovy / 2.f);
    glm::mat4 proj_mat = glm::mat4(0.0f);

    proj_mat[0][0] = 1.f / (aspect * tan_half_fovy);
    proj_mat[1][1] = 1.f / tan_half_fovy;
    proj_mat[2][2] = zfar / (znear - zfar);
    proj_mat[2][3] = -1.f;
    proj_mat[3][2] = -(zfar * znear) / (zfar - znear);

    return proj_mat;
}

glm::mat4 makeOrthographicProjectionMatrix(float left, float right, float bottom, float top, float znear, float zfar)
{
    float inv_width    = 1.0f / (right - left);
    float inv_height   = 1.0f / (top - bottom);
    float inv_distance = 1.0f / (zfar - znear);

    float A  = 2 * inv_width;
    float B  = 2 * inv_height;
    float C  = -(right + left) * inv_width;
    float D  = -(top + bottom) * inv_height;
    float q  = -1 * inv_distance;
    float qn = -znear * inv_distance;

    glm::mat4 orth_matrix = glm::mat4(0.0f);
    orth_matrix[0][0]     = A;
    orth_matrix[0][3]     = C;
    orth_matrix[1][1]     = B;
    orth_matrix[1][3]     = D;
    orth_matrix[2][2]     = q;
    orth_matrix[2][3]     = qn;
    orth_matrix[3][3]     = 1;

    return orth_matrix;
}
