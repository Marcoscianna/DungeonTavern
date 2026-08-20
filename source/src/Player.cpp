#include <json.hpp>

#include "../include/modules/Starter.hpp"
#include "../include/modules/Scene.hpp"
#include "../include/Player.hpp"

Player::Player(glm::vec3 startPos) {
    playerCollider = nullptr;
    init(startPos);
}

Player::~Player() {
    if (playerCollider != nullptr) {
        delete playerCollider;
        playerCollider = nullptr;
    }
}

void Player::init(glm::vec3 startPos, float startYaw, float startPitch, float radius) {
    position = startPos;
    yaw = startYaw;
    pitch = startPitch;
    moveSpeed = 10.0f;
    rotSpeed = 0.1f;
    FOVy = glm::radians(45.0f);
    nearPlane = 0.1f;
    farPlane = 100.0f;
    lastMouseX = 0.0;
    lastMouseY = 0.0;
    mouseLookInitialized = false;

    // Inizializza il collider dinamico
    colliderRadius = radius;
    if (playerCollider == nullptr) {
        playerCollider = new Collider();
    }
    playerCollider->initSphere(0.0f, 0.0f, 0.0f, colliderRadius);
    playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
}

glm::vec3 Player::getForwardVector() const {
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(forward);
}

glm::vec3 Player::getRightVector() const {
    return glm::normalize(glm::cross(getForwardVector(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Player::getUpVector() const {
    return glm::normalize(glm::cross(getRightVector(), getForwardVector()));
}

void Player::updateMouseLook(GLFWwindow* window) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (!mouseLookInitialized) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        mouseLookInitialized = true;
        return;
    }

    float xoffset = static_cast<float>(xpos - lastMouseX);
    float yoffset = static_cast<float>(lastMouseY - ypos);

    lastMouseX = xpos;
    lastMouseY = ypos;

    yaw += xoffset * rotSpeed;
    pitch += yoffset * rotSpeed;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

bool Player::checkCollisionAt(const glm::vec3& testPos, const Scene& scene) {
    glm::mat4 testWm = glm::translate(glm::mat4(1.0f), testPos);
    playerCollider->setWorldMatrix(testWm);

    for (int i = 0; i < scene.InstanceCount; i++) {
        if (scene.I[i]->C != nullptr && playerCollider->collidesWith(*(scene.I[i]->C))) {
            return true;
        }
    }
    return false;
}

void Player::processInput(GLFWwindow* window, float deltaTime, const Scene& scene) {
    // 1. Mouse Look
    updateMouseLook(window);

    float velocity = moveSpeed * deltaTime;
    glm::vec3 forward = getForwardVector();
    glm::vec3 forwardFlat = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    glm::vec3 rightFlat = getRightVector();

    // 2. Calcola posizione desiderata (target)
    glm::vec3 targetPos = position;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        targetPos += forwardFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        targetPos -= forwardFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        targetPos -= rightFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        targetPos += rightFlat * velocity;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        targetPos.y += velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        targetPos.y -= velocity;

    // 3. Risoluzione collisioni asse per asse (Sliding lungo i muri)
    glm::vec3 resolvedPos = position;

    // Test asse X
    glm::vec3 testX = resolvedPos;
    testX.x = targetPos.x;
    if (!checkCollisionAt(testX, scene)) {
        resolvedPos.x = targetPos.x;
    }

    // Test asse Z
    glm::vec3 testZ = resolvedPos;
    testZ.z = targetPos.z;
    if (!checkCollisionAt(testZ, scene)) {
        resolvedPos.z = targetPos.z;
    }

    // Test asse Y
    glm::vec3 testY = resolvedPos;
    testY.y = targetPos.y;
    if (!checkCollisionAt(testY, scene)) {
        resolvedPos.y = targetPos.y;
    }

    // 4. Assegna posizione finale e aggiorna matrice del collider
    position = resolvedPos;
    playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
}

glm::mat4 Player::getViewMatrix() const {
    return glm::lookAt(position, position + getForwardVector(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Player::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 Prj = glm::perspective(FOVy, aspectRatio, nearPlane, farPlane);
    Prj[1][1] *= -1;
    return Prj;
}

glm::mat4 Player::getViewProjectionMatrix(float aspectRatio) const {
    return getProjectionMatrix(aspectRatio) * getViewMatrix();
}