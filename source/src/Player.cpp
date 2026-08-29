#include <json.hpp>

#include "../include/modules/Starter.hpp"
#include "../include/modules/Scene.hpp"
#include "../include/PhysicsManager.hpp"
#include "../include/Player.hpp"

Player::Player(glm::vec3 startPos) {
    playerCollider = nullptr;
    if (startPos.y < 1.51f) {
        startPos.y = 2.0f;
    }
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
    flyMode = false;
    mPressedLastFrame = false;
    playerVelocityY = 0.0f;
    FOVy = glm::radians(45.0f);
    nearPlane = 0.1f;
    farPlane = 500.0f;
    lastMouseX = 0.0;
    lastMouseY = 0.0;
    mouseLookInitialized = false;

    // Inizializza il collider dinamico
    colliderRadius = radius;
    if (playerCollider == nullptr) {
        playerCollider = new Collider();
    }

    playerCollider->initAABB(-0.3f, -3.0f, -0.3f, 0.3f, 0.2f, 0.3f);
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

bool Player::checkCollisionAt(const glm::vec3& testPos, const Scene& scene, const PhysicsManager& physManager) {
    glm::mat4 testWm = glm::translate(glm::mat4(1.0f), testPos);
    playerCollider->setWorldMatrix(testWm);

    if (testPos.y - 3.0f <= 0.0f) {
        return true;
    }

    // 2. Test contro gli oggetti della scena
    int heldObj = physManager.getHeldInstanceIndex();

    for (int i = 0; i < scene.InstanceCount; i++) {

        if (i == heldObj) continue;

        if (scene.I[i]->C != nullptr && playerCollider->collidesWith(*(scene.I[i]->C))) {
            return true;
        }
    }

    // 3. Test contro i muri custom invisibili del JSON
    for (Collider* cld : physManager.getCustomColliders()) {
        if (playerCollider->collidesWith(*cld)) return true;
    }

    return false;
}

void Player::processInput(GLFWwindow* window, float deltaTime, const Scene& scene, const PhysicsManager& physManager) {
    updateMouseLook(window);

    //Limitatore di deltaTime per evitare movimenti troppo grandi in caso di frame rate basso
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // --- TOGGLE FLY MODE ---
    bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mPressed && !mPressedLastFrame) {
        flyMode = !flyMode;
        if (flyMode) playerVelocityY = 0.0f; // Azzera la gravità quando inizi a volare
    }
    mPressedLastFrame = mPressed;

    float velocity = moveSpeed * deltaTime * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f); // Shift per volare/correre 2x più veloce
    glm::vec3 forward = getForwardVector();
    glm::vec3 forwardFlat = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    glm::vec3 rightFlat = getRightVector();

    // --- LOGICA FLY MODE (Nessuna gravità, niente collisioni) ---
    if (flyMode) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= rightFlat * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += rightFlat * velocity;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += velocity*2;

        playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
        return;
    }

    // 1. Raccogli l'input orizzontale (WASD)
    glm::vec3 desiredMove(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) desiredMove += forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) desiredMove -= forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) desiredMove -= rightFlat;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) desiredMove += rightFlat;

    // Normalizza la direzione e moltiplicala per la velocità per evitare la super-velocità in diagonale
    if (glm::length(desiredMove) > 0.01f) {
        desiredMove = glm::normalize(desiredMove) * velocity;
    }

    glm::vec3 resolvedPos = position;
    float stepHeight = 0.6f;

    // SUB-STEPPING
    float moveLength = glm::length(desiredMove);
    int numSteps = (int)(moveLength / 0.1f) + 1;
    glm::vec3 stepMove = desiredMove / (float)numSteps;

    // mini-passi in sequenza
    for (int i = 0; i < numSteps; i++) {

        // Asse X
        glm::vec3 testX = resolvedPos;
        testX.x += stepMove.x;
        if (!checkCollisionAt(testX, scene, physManager)) {
            resolvedPos.x = testX.x;
        } else {
            // Tenta di salire il gradino
            glm::vec3 stepUpX = testX; stepUpX.y += stepHeight;
            if (!checkCollisionAt(stepUpX, scene, physManager)) {
                resolvedPos.x = testX.x;
                resolvedPos.y += stepHeight;
            }
        }

        // Asse Z
        glm::vec3 testZ = resolvedPos;
        testZ.z += stepMove.z;
        if (!checkCollisionAt(testZ, scene, physManager)) {
            resolvedPos.z = testZ.z;
        } else {
            // Tenta di salire il gradino
            glm::vec3 stepUpZ = testZ; stepUpZ.y += stepHeight;
            if (!checkCollisionAt(stepUpZ, scene, physManager)) {
                resolvedPos.z = testZ.z;
                resolvedPos.y += stepHeight;
            }
        }
    }

    // --- 4. Risoluzione Asse Y (Gravità e Salto Reale) ---
    playerVelocityY -= 40.0f * deltaTime; // Applica la forza di gravità

    glm::vec3 testY = resolvedPos;
    testY.y += playerVelocityY * deltaTime; // Calcola dove cadremo

    if (!checkCollisionAt(testY, scene, physManager)) {
        resolvedPos.y = testY.y; // Cadi (o sali in aria se stiamo saltando)
    } else {
        if (playerVelocityY < 0.0f) {
            // Abbiamo toccato terra! (Siamo in piedi su un pavimento o un gradino)
            playerVelocityY = 0.0f;

            // Possiamo saltare SOLO se siamo a terra
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                playerVelocityY = 12.0f; // Forza del salto verso l'alto
            }
        } else {
            // Abbiamo sbattuto la testa saltando
            playerVelocityY = 0.0f;
        }
    }

    // Applica e muovi il collider
    position = resolvedPos;
    playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position)); //
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