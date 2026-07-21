#include "../include/Player.hpp"

Player::Player(glm::vec3 startPos) {
    init(startPos);
}

void Player::init(glm::vec3 startPos, float startYaw, float startPitch) {
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
}

// Update the player's position and orientation based on input
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
    float yoffset = static_cast<float>(lastMouseY - ypos); // invertito: coordinate Y vanno dal basso verso l'alto

    lastMouseX = xpos;
    lastMouseY = ypos;

    yaw += xoffset * rotSpeed;
    pitch += yoffset * rotSpeed;

    // Limita il pitch per evitare il ribaltamento della telecamera
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

void Player::processInput(GLFWwindow* window, float deltaTime) {
    // Aggiorna prima l'orientamento con il mouse
    updateMouseLook(window);

    float velocity = moveSpeed * deltaTime;

    // Movimento orizzontale
    glm::vec3 forward = getForwardVector();
    glm::vec3 forwardFlat = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    glm::vec3 rightFlat = getRightVector();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += forwardFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= forwardFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= rightFlat * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += rightFlat * velocity;

    // Movimento verticale
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        position.y += velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        position.y -= velocity;
}

glm::mat4 Player::getViewMatrix() const {
    return glm::lookAt(position, position + getForwardVector(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Player::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 Prj = glm::perspective(FOVy, aspectRatio, nearPlane, farPlane);
    Prj[1][1] *= -1; // Inversione Y per il sistema di coordinate Vulkan
    return Prj;
}

glm::mat4 Player::getViewProjectionMatrix(float aspectRatio) const {
    return getProjectionMatrix(aspectRatio) * getViewMatrix();
}