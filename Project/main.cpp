#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"

#include <iostream>
#include <vector>
#include <unordered_map>

// ------Camera------
gps::Camera myCamera(
    glm::vec3(-36.5f, 2.0f, 114.5f),
    glm::vec3(0.0f, 2.0f, -10.0f),
    glm::vec3(0.0f, 1.0f, 0.0f)
);
GLfloat cameraSpeed = 0.05f;
float yaw = -90.0f;
float pitch = 0.0f;
float cameraRadius = 0.3f;
// Mouse control
bool  firstMouse = true;
float lastX = 512.0f;
float lastY = 384.0f;
float sensitivity = 0.1f;


// ------Presentation------
bool isPresentationMode = false;
int  currentPathIndex = 0;
// Movement & rotation
float presentationSpeed = 0.02f;
float presentationRotationSpeed = 0.01f;
// Pause control
bool  isPaused = false;
float pauseStartTime = 0.0f;
float pauseDuration = 1.0f;
// State timing
float timer = 0.0f;
glm::vec3 savedDirection;
// Stop and look around points
enum P1State{ P1_NONE, P1_LOOK, P1_PAUSE, P1_LOOK_BACK, P1_DONE };
enum P3State{ P3_NONE, P3_LOOK_LEFT, P3_LOOK, P3_PAUSE, P3_LOOK_BACK, P3_DONE };
enum P5State{ P5_NONE, P5_LOOK, P5_PAUSE, P5_LOOK_BACK, P5_DONE };
enum P14State{ P14_NONE, P14_LOOK, P14_PAUSE, P14_LOOK_BACK, P14_DONE };
enum P15State{ P15_NONE, P15_PAUSE, P15_TURN_180, P15_DONE };
P1State  currentP1State = P1_NONE;
P3State  currentP3State = P3_NONE;
P5State  currentP5State = P5_NONE;
P14State currentP14State = P14_NONE;
P15State currentP15State = P15_NONE;
// Presentation Path for Camera
std::vector<glm::vec3> presentationPath ={
    glm::vec3(-36.5f, 2.0f, 114.5f),  // Point 0: Start
    glm::vec3(-36.5f, 2.0f, 98.5f),  // Point 1: Walk forward
    glm::vec3(-29.5f, 2.0f, 98.5f),   // Point 2: Walk right
    glm::vec3(-29.5f, 2.0f, 83.5f),   // Point 3: Walk forward
    glm::vec3(-60.0f, 2.0f, 83.5f),   // Point 4: Walk left
    glm::vec3(-59.5f, 2.0f, 101.5f),   // Point 5: See cell
    glm::vec3(-59.42f, 2.0f, 129.5f),   // Point 6: Enter turn
    glm::vec3(-58.5f, 2.0f, 130.4f),   // Point 7: Leave turn
    glm::vec3(-55.5f, 2.0f, 130.5f),   // Point 8: Enter staircase
    glm::vec3(-45.5f, 5.0f, 130.5f),   // Point 9: Leave staircase
    glm::vec3(-43.5f, 5.0f, 130.4f),   // Point 10: Enter turn
    glm::vec3(-42.58f, 5.0f, 129.5f),   // Point 11: Leave turn
    glm::vec3(-42.5f, 5.0f, 127.5f),   // Point 12: Enter staircase
    glm::vec3(-42.5f, 8.0f, 119.5f),   // Point 13: Leave staircase
    glm::vec3(-42.5f, 8.0f, 94.5f),  // Point 14: See skeleton
    glm::vec3(-42.5f, 8.0f, 62.0f),  // Point 15: Walk forward
};


// ------Ghost------
glm::vec3 ghostPos ={ -36.5f, 0.5f, 83.5f };
bool ghostVisible = true;
float ghostSpeed = 0.07f;
float ghostRotationAngle = -90.0f;

bool ghostMovingToTarget1 = false;
bool ghostMovingToTarget2 = false;
bool ghostMovingToTarget3 = false;
glm::vec3 ghostTarget1 ={ -59.5f, 0.5f, 83.5f };
glm::vec3 ghostTarget2 ={ -59.5f, 0.5f, 88.5f };
glm::vec3 ghostTarget3 ={ -42.5f, 6.5f, 62.0f };


// ------Flashlight------
glm::vec3 lightPos;
glm::vec3 lightDir;
glm::vec3 lightColor;
GLuint lightPosLoc;
GLuint lightDirLoc;
GLuint lightColorLoc;
bool flashlightOn = true;

// ------Torch------
glm::vec3 torchPos ={ -40.14f, 7.05f, 81.3f };
glm::vec3 torchColor ={ 1.0f,  0.5f,  0.0f };
GLuint torchPosLoc;
GLuint torchColorLoc;

// ------Shadow Mapping------
const unsigned int SHADOW_WIDTH = 8192;
const unsigned int SHADOW_HEIGHT = 8192;

GLuint shadowMapFBO;
GLuint depthMapTexture;

GLuint torchShadowMapFBO;
GLuint torchDepthMapTexture;


// ------Collision System------
struct Triangle{
    glm::vec3 v0, v1, v2;
};

struct SpatialGrid{
    float cellSize;
    std::unordered_map<int64_t, std::vector<Triangle>> grid;

    int64_t hashPos(int x, int y, int z){
        return ((int64_t)x << 32) | ((int64_t)y << 16) | (int64_t)z;
    }

    void insert(const Triangle& tri){
        glm::vec3 minPos = glm::min(glm::min(tri.v0, tri.v1), tri.v2);
        glm::vec3 maxPos = glm::max(glm::max(tri.v0, tri.v1), tri.v2);

        int minX = (int)floor(minPos.x / cellSize);
        int minY = (int)floor(minPos.y / cellSize);
        int minZ = (int)floor(minPos.z / cellSize);
        int maxX = (int)floor(maxPos.x / cellSize);
        int maxY = (int)floor(maxPos.y / cellSize);
        int maxZ = (int)floor(maxPos.z / cellSize);

        for (int x = minX; x <= maxX; x++){
            for (int y = minY; y <= maxY; y++){
                for (int z = minZ; z <= maxZ; z++){
                    grid[hashPos(x, y, z)].push_back(tri);
                }
            }
        }
    }

    std::vector<Triangle> queryNearby(glm::vec3 pos, float radius){
        std::vector<Triangle> nearby;

        int minX = (int)floor((pos.x - radius) / cellSize);
        int minY = (int)floor((pos.y - radius) / cellSize);
        int minZ = (int)floor((pos.z - radius) / cellSize);
        int maxX = (int)floor((pos.x + radius) / cellSize);
        int maxY = (int)floor((pos.y + radius) / cellSize);
        int maxZ = (int)floor((pos.z + radius) / cellSize);

        for (int x = minX; x <= maxX; x++){
            for (int y = minY; y <= maxY; y++){
                for (int z = minZ; z <= maxZ; z++){
                    auto it = grid.find(hashPos(x, y, z));
                    if (it != grid.end()){
                        nearby.insert(nearby.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }

        return nearby;
    }
};

std::vector<Triangle> allCollisionTriangles;
SpatialGrid spatialGrid;
GLuint wireframeVAO = 0;
GLuint wireframeVBO = 0;
int wireframeVertexCount = 0;
bool showCollisionMesh = false;


// ------Matrices------
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;
GLuint modelLoc;
GLuint viewLoc;
GLuint projectionLoc;
GLuint normalMatrixLoc;


// -----Models Shaders------
gps::Model3D tileSet;
gps::Model3D ghostModel;

gps::Shader myBasicShader;
gps::Shader wireframeShader;
gps::Shader depth2DShader;
gps::Shader depthCubeShader;

GLfloat angle;

// ------Window Iput------
gps::Window myWindow;
GLboolean pressedKeys[1024];

glm::mat4 computeLightSpaceTrMatrix(){
    const GLfloat near_plane = 0.1f, far_plane = 50.0f;

    glm::mat4 lightProjection = glm::perspective(glm::radians(60.0f), 1.0f, near_plane, far_plane);
    glm::vec3 lightTarget = lightPos + lightDir;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::abs(glm::dot(glm::normalize(lightDir), up)) > 0.95f){
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, up);

    return lightProjection * lightView;
}

GLenum glCheckError_(const char* file, int line){
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR){
        std::string error;
        switch (errorCode){
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height){
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
}

void drawObjects(gps::Shader shader, bool depthPass){
    shader.useShaderProgram();

    // Draw TileSet
    glm::mat4 tileSetModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(tileSetModel));

    if (!depthPass){
        glm::mat3 normalMatrix = glm::mat3(glm::inverseTranspose(view * tileSetModel));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }

    tileSet.Draw(shader);


    // Draw Ghost
    if (ghostVisible){
        glm::mat4 ghostMatrix = glm::translate(glm::mat4(1.0f), ghostPos);
        ghostMatrix = glm::rotate(ghostMatrix, glm::radians(ghostRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));

        glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(ghostMatrix));

        if (!depthPass){
            glm::mat3 normalMatrix = glm::mat3(glm::inverseTranspose(view * ghostMatrix));
            glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
        }

        ghostModel.Draw(shader);
    }
}

void updateFlashlight(){
    float forwardOffset = 0.5f;
    float downOffset = 0.5f;
    float tiltAmount = 0.5f;

    lightPos = myCamera.cameraPosition + (myCamera.cameraFrontDirection * forwardOffset) - (glm::vec3(0.0f, 1.0f, 0.0f) * downOffset);

    float tiltDown = 0.00005f;
    lightDir = glm::normalize(myCamera.cameraFrontDirection - (glm::vec3(0.0f, 1.0f, 0.0f) * tiltDown));

    myBasicShader.useShaderProgram();
    glm::vec3 currentFlashlightColor = flashlightOn ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(0.0f, 0.0f, 0.0f);
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(currentFlashlightColor));
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(currentFlashlightColor));
    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
}

void updatePresentation(){
    if (!isPresentationMode) return;

    if (ghostMovingToTarget1){
        if (glm::distance(ghostPos, ghostTarget1) > 0.1f) 
            ghostPos += glm::normalize(ghostTarget1 - ghostPos) * ghostSpeed;
        else ghostMovingToTarget1 = false;
    }
    if (ghostMovingToTarget2){
        if (glm::distance(ghostPos, ghostTarget2) > 0.1f) 
            ghostPos += glm::normalize(ghostTarget2 - ghostPos) * ghostSpeed;
        else{ 
            ghostMovingToTarget2 = false; 
            ghostVisible = false; 
        }
    }
    if (ghostMovingToTarget3){
        if (glm::distance(ghostPos, ghostTarget3) > 0.1f)
            ghostPos += glm::normalize(ghostTarget3 - ghostPos) * ghostSpeed;
        else
            ghostMovingToTarget3 = false;
    }

    if (currentPathIndex >= presentationPath.size() ||
        (currentP15State == P15_DONE && !ghostMovingToTarget3 && currentPathIndex == 15)){

        if (!ghostMovingToTarget3){
            isPresentationMode = false;
            ghostVisible = false;
            glm::vec3 front = glm::normalize(myCamera.cameraFrontDirection);
            yaw = glm::degrees(atan2(front.z, front.x));
            pitch = glm::degrees(asin(front.y));
            std::cout << "Presentation Mode: END" << std::endl;
            return;
        }
    }

    // ------POINT 1 SEQUENCE------
    if (currentP1State != P1_NONE && currentP1State != P1_DONE){
        if (currentP1State == P1_LOOK){
            glm::vec3 leftDir = glm::normalize(glm::cross(glm::vec3(0, 1, 0), savedDirection));
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, leftDir, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, leftDir) > 0.499f){
                currentP1State = P1_PAUSE;
                timer = glfwGetTime();
            }
        }
        else if (currentP1State == P1_PAUSE){
            if (glfwGetTime() - timer > 1.0f) currentP1State = P1_LOOK_BACK;
        }
        else if (currentP1State == P1_LOOK_BACK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, savedDirection, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, savedDirection) > 0.999f){
                currentP1State = P1_DONE;
                currentPathIndex++;
            }
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        return;
    }

    // ------POINT 3 SEQUENCE------
    if (currentP3State != P3_NONE && currentP3State != P3_DONE){
        glm::vec3 leftDir = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), savedDirection));
        glm::vec3 diagonalDir = glm::normalize(leftDir + savedDirection + glm::vec3(0.0f, 1.0f, 0.0f));

        if (currentP3State == P3_LOOK_LEFT){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, leftDir, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, leftDir) > 0.999f){
                currentP3State = P3_LOOK;
            }
        }
        else if (currentP3State == P3_LOOK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, diagonalDir, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, diagonalDir) > 0.929f){
                currentP3State = P3_PAUSE;
                timer = glfwGetTime();
            }
        }
        else if (currentP3State == P3_PAUSE){
            if (glfwGetTime() - timer > 1.0f){
                currentP3State = P3_LOOK_BACK;
            }
        }
        else if (currentP3State == P3_LOOK_BACK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, leftDir, presentationRotationSpeed));

            if (glm::dot(myCamera.cameraFrontDirection, leftDir) > 0.999f){
                currentP3State = P3_DONE;
                currentPathIndex++;
            }
        }

        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        return;
    }

    // ------POINT 5 SEQUENCE------
    if (currentP5State != P5_NONE && currentP5State != P5_DONE){
        if (currentP5State == P5_LOOK){
            glm::vec3 leftDir = glm::normalize(glm::cross(glm::vec3(0, 1, 0), savedDirection));
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, leftDir, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, leftDir) > 0.989f){
                currentP5State = P5_PAUSE;
                timer = glfwGetTime();
            }
        }
        else if (currentP5State == P5_PAUSE){
            if (glfwGetTime() - timer > 1.0f) currentP5State = P5_LOOK_BACK;
        }
        else if (currentP5State == P5_LOOK_BACK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, savedDirection, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, savedDirection) > 0.999f){
                currentP5State = P5_DONE;
                currentPathIndex++;
            }
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        return;
    }

    // ------POINT 14 SEQUENCE------
    if (currentP14State != P14_NONE && currentP14State != P14_DONE){
        glm::vec3 rightDir = glm::normalize(glm::cross(savedDirection, glm::vec3(0, 1, 0)));
        glm::vec3 diagonalDir = glm::normalize(savedDirection + rightDir + glm::vec3(0, 1, 0));

        if (currentP14State == P14_LOOK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, diagonalDir, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, diagonalDir) > 0.959f){
                currentP14State = P14_PAUSE;
                timer = glfwGetTime();
            }
        }
        else if (currentP14State == P14_PAUSE){
            if (glfwGetTime() - timer > 1.0f) currentP14State = P14_LOOK_BACK;
        }
        else if (currentP14State == P14_LOOK_BACK){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, savedDirection, presentationRotationSpeed));
            if (glm::dot(myCamera.cameraFrontDirection, savedDirection) > 0.999f){
                currentP14State = P14_DONE;
                currentPathIndex++;
            }
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        return;
    }

    // ------POINT 15 SEQUENCE------
    if (currentP15State != P15_NONE && currentP15State != P15_DONE){
        glm::vec3 behindDir = -savedDirection;

        if (currentP15State == P15_PAUSE){
            if (glfwGetTime() - timer > 1.0f){
                currentP15State = P15_TURN_180;
            }
        }
        else if (currentP15State == P15_TURN_180){
            myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, behindDir, 0.05f));

            if (glm::dot(myCamera.cameraFrontDirection, behindDir) > 0.999f){

                myCamera.cameraFrontDirection = behindDir;
                currentP15State = P15_DONE;
                ghostMovingToTarget3 = true;
            }
        }

        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        return;
    }

    // ------NORMAL MOVEMENT--------
    if (currentPathIndex <= presentationPath.size()){
        glm::vec3 targetPos = presentationPath[currentPathIndex];
        if (currentPathIndex == 4){
            float totalDist = glm::distance(presentationPath[3], presentationPath[4]);
            float currentDist = glm::distance(myCamera.cameraPosition, presentationPath[4]);
            if (currentDist < (totalDist * 0.5f)) ghostMovingToTarget2 = true;
        }

        glm::vec3 desiredDir = glm::normalize(targetPos - myCamera.cameraPosition);
        myCamera.cameraFrontDirection = glm::normalize(glm::mix(myCamera.cameraFrontDirection, desiredDir, presentationRotationSpeed));

        float distance = glm::distance(myCamera.cameraPosition, targetPos);
        if (distance < 2.5f && currentPathIndex == 1){
            ghostMovingToTarget1 = true;
        }
        if (distance < 0.5f){
            // Trigger Point 1
            if (currentPathIndex == 1){
                currentP1State = P1_LOOK;
                savedDirection = desiredDir;
                return;
            }
            // Trigger Point 3
            if (currentPathIndex == 3 && currentP3State == P3_NONE){
                currentP3State = P3_LOOK_LEFT;
                savedDirection = desiredDir;
                return;
            }
            // Trigger Point 5
            if (currentPathIndex == 5 && currentP5State == P5_NONE){
                currentP5State = P5_LOOK;
                savedDirection = desiredDir;
                return;
            }
            // Trigger Point 14
            if (currentPathIndex == 14 && currentP14State == P14_NONE){
                currentP14State = P14_LOOK;
                savedDirection = desiredDir;
                return;
            }
            // Trigger Point 15
            if (currentPathIndex == 15 && currentP15State == P15_NONE){
                currentP15State = P15_PAUSE;
                timer = glfwGetTime();
                savedDirection = desiredDir;

                ghostVisible = true;
                ghostPos = glm::vec3(-42.5f, 6.5f, 72.0f);
                ghostRotationAngle = 180.0f;

                return;
            }
            if (currentPathIndex < presentationPath.size() - 1){
                currentPathIndex++;
            }
        }
        else{
            myCamera.cameraPosition += desiredDir * presentationSpeed;
            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos){
    if (isPresentationMode) return;

    if (firstMouse){
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    myCamera.rotate(pitch, yaw);

    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    updateFlashlight();
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode){
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS){
        if (!presentationPath.empty()){
            isPresentationMode = true;

            myCamera.cameraPosition = presentationPath[0];

            glm::vec3 startTarget = presentationPath[1];
            glm::vec3 direction = glm::normalize(startTarget - myCamera.cameraPosition);
            myCamera.cameraFrontDirection = direction;
            myCamera.cameraUpDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            myCamera.cameraRightDirection = glm::normalize(glm::cross(myCamera.cameraFrontDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
            pitch = 0.0f;
            yaw = glm::degrees(atan2(direction.z, direction.x));

            currentPathIndex = 1;

            currentP1State = P1_NONE;
            currentP3State = P3_NONE;
            currentP5State = P5_NONE;
            currentP14State = P14_NONE;
            currentP15State = P15_NONE;

            ghostPos = glm::vec3(-36.5f, 0.5f, 83.5f);
            ghostRotationAngle = -90.0f;
            ghostVisible = true;
            ghostMovingToTarget1 = false;
            ghostMovingToTarget2 = false;
            ghostMovingToTarget3 = false;
            isPaused = false;

            view = myCamera.getViewMatrix();

            std::cout << "Presentation Mode: START" << std::endl;
        }
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS){
        showCollisionMesh = !showCollisionMesh;
        std::cout << "Collision mesh wireframe: " << (showCollisionMesh ? "ON" : "OFF") << std::endl;
    }

    if (key >= 0 && key < 1024){
        if (action == GLFW_PRESS){
            pressedKeys[key] = true;
        }
        else if (action == GLFW_RELEASE){
            pressedKeys[key] = false;
        }
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS){
        flashlightOn = !flashlightOn;
        std::cout << "Flashlight: " << (flashlightOn ? "ON" : "OFF") << std::endl;
    }
}

float pointToTriangleDistance(glm::vec3 point, const Triangle& tri){
    glm::vec3 edge0 = tri.v1 - tri.v0;
    glm::vec3 edge1 = tri.v2 - tri.v0;

    glm::vec3 normal = glm::normalize(glm::cross(edge0, edge1));
    float distToPlane = glm::abs(glm::dot(point - tri.v0, normal));

    if (distToPlane > cameraRadius){
        return distToPlane;
    }

    glm::vec3 projPoint = point - normal * glm::dot(point - tri.v0, normal);

    glm::vec3 minBounds = glm::min(glm::min(tri.v0, tri.v1), tri.v2) - glm::vec3(cameraRadius);
    glm::vec3 maxBounds = glm::max(glm::max(tri.v0, tri.v1), tri.v2) + glm::vec3(cameraRadius);

    if (projPoint.x < minBounds.x || projPoint.x > maxBounds.x ||
        projPoint.y < minBounds.y || projPoint.y > maxBounds.y ||
        projPoint.z < minBounds.z || projPoint.z > maxBounds.z){
        return cameraRadius + 1.0f;
    }

    return distToPlane;
}

bool checkMeshCollision(glm::vec3 position){
    glm::mat4 inverseModel = glm::inverse(model);
    glm::vec3 localPos = glm::vec3(inverseModel * glm::vec4(position, 1.0f));

    std::vector<Triangle> nearbyTris = spatialGrid.queryNearby(localPos, cameraRadius);

    for (const auto& tri : nearbyTris){
        float dist = pointToTriangleDistance(localPos, tri);
        if (dist < cameraRadius){
            return true;
        }
    }

    return false;
}

void extractCollisionMesh(gps::Model3D& model3d){
    std::cout << "Extracting collision mesh..." << std::endl;

    spatialGrid.cellSize = 1.0f;

    int totalTriangles = 0;

    for (const auto& mesh : model3d.meshes){
        for (size_t i = 0; i < mesh.indices.size(); i += 3){
            if (i + 2 < mesh.indices.size()){
                Triangle tri;
                tri.v0 = mesh.vertices[mesh.indices[i]].Position;
                tri.v1 = mesh.vertices[mesh.indices[i + 1]].Position;
                tri.v2 = mesh.vertices[mesh.indices[i + 2]].Position;

                spatialGrid.insert(tri);
                allCollisionTriangles.push_back(tri);
                totalTriangles++;
            }
        }
    }

    std::cout << "Extracted " << totalTriangles << " triangles into spatial grid" << std::endl;
    std::cout << "Grid cells used: " << spatialGrid.grid.size() << std::endl;
}

void setupWireframeBuffers(){
    std::vector<glm::vec3> vertices;

    for (const auto& tri : allCollisionTriangles){
        vertices.push_back(tri.v0);
        vertices.push_back(tri.v1);
        vertices.push_back(tri.v1);
        vertices.push_back(tri.v2);
        vertices.push_back(tri.v2);
        vertices.push_back(tri.v0);
    }

    wireframeVertexCount = vertices.size();

    glGenVertexArrays(1, &wireframeVAO);
    glGenBuffers(1, &wireframeVBO);

    glBindVertexArray(wireframeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);

    std::cout << "Wireframe buffer created with " << wireframeVertexCount << " vertices" << std::endl;
}

void initShadowFramebuffer(){
    glGenFramebuffers(1, &shadowMapFBO);

    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    float borderColor[] ={ 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "Shadow framebuffer initialized" << std::endl;
}

void initTorchShadow(){
    glGenFramebuffers(1, &torchShadowMapFBO);

    glGenTextures(1, &torchDepthMapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, torchDepthMapTexture);

    for (unsigned int i = 0; i < 6; ++i){
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
            SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, torchShadowMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, torchDepthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void processMovement(){
    if (isPresentationMode){
        updatePresentation();
        updateFlashlight();
        return;
    }

    glm::vec3 oldCameraPos = myCamera.cameraPosition;
    bool cameraMoved = false;

    if (pressedKeys[GLFW_KEY_W]){
        myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
        if (checkMeshCollision(myCamera.cameraPosition)){
            myCamera.cameraPosition = oldCameraPos;
        }
        else{
            cameraMoved = true;
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_S]){
        myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
        if (checkMeshCollision(myCamera.cameraPosition)){
            myCamera.cameraPosition = oldCameraPos;
        }
        else{
            cameraMoved = true;
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_A]){
        myCamera.move(gps::MOVE_LEFT, cameraSpeed);
        if (checkMeshCollision(myCamera.cameraPosition)){
            myCamera.cameraPosition = oldCameraPos;
        }
        else{
            cameraMoved = true;
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_D]){
        myCamera.move(gps::MOVE_RIGHT, cameraSpeed);
        if (checkMeshCollision(myCamera.cameraPosition)){
            myCamera.cameraPosition = oldCameraPos;
        }
        else{
            cameraMoved = true;
        }
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (cameraMoved){
        updateFlashlight();
    }

    if (pressedKeys[GLFW_KEY_Q]){
        angle -= 1.0f;
    }

    if (pressedKeys[GLFW_KEY_E]){
        angle += 1.0f;
    }
}

void initOpenGLWindow(){
    if (!glfwInit()){
        fprintf(stderr, "Failed to initialize GLFW\n");
        return;
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    myWindow.Create(mode->width, mode->height, "OpenGL Project Core", primaryMonitor);

    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void setWindowCallbacks(){
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
}

void initOpenGLState(){
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void initModels(){
    tileSet.LoadModel("models/TileSet/TileSetGP.obj");
    ghostModel.LoadModel("models/Ghost/Ghost.obj");

    extractCollisionMesh(tileSet);
    setupWireframeBuffers();
}

void initShaders(){
    myBasicShader.loadShader("shaders/basic.vert", "shaders/basic.frag");
    wireframeShader.loadShader("shaders/wireframe.vert", "shaders/wireframe.frag");

    depth2DShader.loadShader(
        "shaders/depthMap2D.vert",
        "shaders/depthMap2D.frag"
    );

    depthCubeShader.loadShader(
        "shaders/depthMapCube.vert",
        "shaders/depthMapCube.frag"
    );
}

void initUniforms(){
    myBasicShader.useShaderProgram();

    model = glm::mat4(1.0f);
    modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    projection = glm::perspective(glm::radians(60.0f),
        (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
        0.1f, 500.0f);
    projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    lightPos = myCamera.cameraPosition;
    lightPosLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightPos");
    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));

    lightDir = myCamera.cameraFrontDirection;
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    GLuint lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix");
}

void renderCollisionWireframe(){
    if (!showCollisionMesh || wireframeVertexCount == 0) return;

    wireframeShader.useShaderProgram();

    GLint modelLocWire = glGetUniformLocation(wireframeShader.shaderProgram, "model");
    GLint viewLocWire = glGetUniformLocation(wireframeShader.shaderProgram, "view");
    GLint projectionLocWire = glGetUniformLocation(wireframeShader.shaderProgram, "projection");
    GLint colorLoc = glGetUniformLocation(wireframeShader.shaderProgram, "wireColor");

    glUniformMatrix4fv(modelLocWire, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLocWire, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLocWire, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f);

    glBindVertexArray(wireframeVAO);
    glDrawArrays(GL_LINES, 0, wireframeVertexCount);
    glBindVertexArray(0);
}

void renderScene(){

    /// ---PASS 1: FLASHLIGHT SHADOW MAP (2D)---
    glm::mat4 flashlightMatrix = computeLightSpaceTrMatrix();

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    depth2DShader.useShaderProgram();
    glUniformMatrix4fv(
        glGetUniformLocation(depth2DShader.shaderProgram, "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(flashlightMatrix)
    );

    drawObjects(depth2DShader, true);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---PASS 2: TORCH SHADOW MAP (CUBEMAP)---
    static bool torchShadowsComputed = false;
    if (!torchShadowsComputed){

        float near_plane = 0.1f;
        float far_plane = 150.0f;
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);

        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(torchPos, torchPos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)));

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, torchShadowMapFBO);

        depthCubeShader.useShaderProgram();
        glUniform1f(
            glGetUniformLocation(depthCubeShader.shaderProgram, "far_plane"),
            far_plane
        );
        glUniform3fv(
            glGetUniformLocation(depthCubeShader.shaderProgram, "lightPos"),
            1, glm::value_ptr(torchPos)
        );

        for (unsigned int i = 0; i < 6; i++){
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                torchDepthMapTexture,
                0
            );
            glClear(GL_DEPTH_BUFFER_BIT);

            glUniformMatrix4fv(
                glGetUniformLocation(depthCubeShader.shaderProgram, "lightSpaceTrMatrix"),
                1, GL_FALSE, glm::value_ptr(shadowTransforms[i])
            );

            drawObjects(depthCubeShader, true);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        torchShadowsComputed = true;
    }

    // ---PASS 3: Main Render---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    myBasicShader.useShaderProgram();

    view = myCamera.getViewMatrix();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glUniformMatrix4fv(
        glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(flashlightMatrix)
    );

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, torchDepthMapTexture);
    glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "torchShadowMap"), 4);

    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "torchPos"), 1, glm::value_ptr(torchPos));
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "torchColor"), 1, glm::value_ptr(torchColor));
    glUniform1f(glGetUniformLocation(myBasicShader.shaderProgram, "far_plane"), 150.0f);

    drawObjects(myBasicShader, false);
    renderCollisionWireframe();
}

void cleanup(){
    if (wireframeVAO != 0){
        glDeleteVertexArrays(1, &wireframeVAO);
        glDeleteBuffers(1, &wireframeVBO);
    }
    if (shadowMapFBO != 0){
        glDeleteFramebuffers(1, &shadowMapFBO);
        glDeleteTextures(1, &depthMapTexture);
    }
    myWindow.Delete();
}

int main(int argc, const char* argv[]){

    try{
        initOpenGLWindow();
    }
    catch (const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    initOpenGLState();
    initModels();
    initShaders();
    initUniforms();
    initTorchShadow();
    initShadowFramebuffer();
    setWindowCallbacks();

    glCheckError();

    std::cout << "Controls:" << std::endl;
    std::cout << "WASD - Move camera" << std::endl;
    std::cout << "F - Toggle flashlight" << std::endl;
    std::cout << "C - Toggle collision wireframe" << std::endl;
    std::cout << "ESC - Exit" << std::endl;

    while (!glfwWindowShouldClose(myWindow.getWindow())){
        processMovement();
        updateFlashlight();
        renderScene();
        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}