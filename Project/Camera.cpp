#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace gps{

    Camera::Camera(glm::vec3 cameraPosition, glm::vec3 cameraTarget, glm::vec3 cameraUp){
        this->cameraPosition = cameraPosition;
        this->cameraTarget = cameraTarget;
        this->cameraFrontDirection = glm::normalize(cameraTarget - cameraPosition);
        this->cameraUpDirection = glm::normalize(cameraUp);
        this->cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, cameraUpDirection));
    }

    glm::mat4 Camera::getViewMatrix(){
        return glm::lookAt(cameraPosition, cameraPosition + cameraFrontDirection, cameraUpDirection);
    }

    void Camera::move(MOVE_DIRECTION direction, float speed){
        if (direction == MOVE_FORWARD){
            cameraPosition += cameraFrontDirection * speed;
        }
        if (direction == MOVE_BACKWARD){
            cameraPosition -= cameraFrontDirection * speed;
        }
        if (direction == MOVE_LEFT){
            cameraPosition -= cameraRightDirection * speed;
        }
        if (direction == MOVE_RIGHT){
            cameraPosition += cameraRightDirection * speed;
        }

        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    void Camera::rotate(float pitch, float yaw){
        glm::vec3 dir;
        dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir.y = sin(glm::radians(pitch));
        dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        cameraFrontDirection = glm::normalize(dir);
        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUpDirection = glm::normalize(glm::cross(cameraRightDirection, cameraFrontDirection));
        cameraTarget = cameraPosition + cameraFrontDirection;
    }
}
