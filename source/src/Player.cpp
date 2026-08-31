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
    cameraMode = 0;
    cPressedLastFrame = false;
    isThirdPerson = false;
    xPressedLastFrame = false;
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

        if (scene.I[i]->id != nullptr) {
            std::string objId = *(scene.I[i]->id);
            // Salta la collisione se l'ID contiene la parola "player" o "Player"
            if (objId.find("player") != std::string::npos || objId.find("Player") != std::string::npos) {
                continue;
            }
        }

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
    // --- DEFINIZIONE LIMITI RETTANGOLO TAVERNA (3D: X, Y, Z) ---
    const float minX = 1.11968f;
    const float maxX = 19.5197f;
    const float minZ = -5.76789f;
    const float maxZ = 21.0792f;
    const float minY = -1.0f;
    const float maxY = 16.0f;

    // Controlla se la posizione del player ricade nel rettangolo
    bool isInTavernRoom = (position.x >= minX && position.x <= maxX) &&
                          (position.y >= minY && position.y <= maxY) &&
                          (position.z >= minZ && position.z <= maxZ);

    // --- TOGGLE TELECAMERA FISSA TAVERNA (TASTO C: 0 -> 1 -> 2 -> 0) ---
    bool cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cPressed && !cPressedLastFrame) {
        if (isInTavernRoom || isFixedCamera()) {
            cameraMode = (cameraMode + 1) % 3; // Cicla tra 0, 1 e 2
            if (cameraMode != 0) {
                isThirdPerson = false; // Disattiva la 3rd person se si passa alle cam fisse
            }
        }
    }
    cPressedLastFrame = cPressed;

    // --- TOGGLE TERZA PERSONA STILE FORTNITE (TASTO X) ---
    bool xPressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
    if (xPressed && !xPressedLastFrame) {
        isThirdPerson = !isThirdPerson;
        if (isThirdPerson) {
            cameraMode = 0; // Torna alla modalita dinamica sbloccata
        }
    }
    xPressedLastFrame = xPressed;

    // Aggiorna orientamento mouse
    updateMouseLook(window);

    // Limitatore di deltaTime per evitare salti in caso di cali di frame rate
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // --- TOGGLE FLY MODE (TASTO M) ---
    bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mPressed && !mPressedLastFrame) {
        flyMode = !flyMode;
        if (flyMode) playerVelocityY = 0.0f; // Azzera la gravita
    }
    mPressedLastFrame = mPressed;

    float velocity = moveSpeed * deltaTime * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f);
    glm::vec3 forward = getForwardVector();
    glm::vec3 forwardFlat = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    glm::vec3 rightFlat = getRightVector();

    // --- LOGICA FLY MODE ---
    if (flyMode) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= rightFlat * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += rightFlat * velocity;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += velocity * 2.0f;

        playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
        return;
    }

    // 1. Input orizzontale (WASD)
    glm::vec3 desiredMove(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) desiredMove += forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) desiredMove -= forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) desiredMove -= rightFlat;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) desiredMove += rightFlat;

    if (glm::length(desiredMove) > 0.01f) {
        desiredMove = glm::normalize(desiredMove) * velocity;
    }

    glm::vec3 resolvedPos = position;
    float stepHeight = 0.6f;

    // SUB-STEPPING
    float moveLength = glm::length(desiredMove);
    int numSteps = static_cast<int>(moveLength / 0.1f) + 1;
    glm::vec3 stepMove = desiredMove / static_cast<float>(numSteps);

    for (int i = 0; i < numSteps; i++) {
        // Asse X
        glm::vec3 testX = resolvedPos;
        testX.x += stepMove.x;
        if (!checkCollisionAt(testX, scene, physManager)) {
            resolvedPos.x = testX.x;
        } else {
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
            glm::vec3 stepUpZ = testZ; stepUpZ.y += stepHeight;
            if (!checkCollisionAt(stepUpZ, scene, physManager)) {
                resolvedPos.z = testZ.z;
                resolvedPos.y += stepHeight;
            }
        }
    }

    // Gravita e salto
    playerVelocityY -= 40.0f * deltaTime;

    glm::vec3 testY = resolvedPos;
    testY.y += playerVelocityY * deltaTime;

    if (!checkCollisionAt(testY, scene, physManager)) {
        resolvedPos.y = testY.y;
    } else {
        if (playerVelocityY < 0.0f) {
            playerVelocityY = 0.0f;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                playerVelocityY = 12.0f;
            }
        } else {
            playerVelocityY = 0.0f;
        }
    }

    // Aggiorna posizione finale e collider
    position = resolvedPos;
    playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
}

glm::mat4 Player::getViewMatrix() const {
    // 1. Telecamere fisse della Taverna (Tasto C)
    if (cameraMode == 1) {
        return glm::lookAt(fixedCamPos1, fixedCamTarget1, glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (cameraMode == 2) {
        return glm::lookAt(fixedCamPos2, fixedCamTarget2, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // 2. Terza Persona stile Fortnite (Tasto X)
    if (isThirdPerson) {
        glm::vec3 forward = getForwardVector();
        glm::vec3 right = getRightVector();
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        float distanceBehind = 4.5f; // Distanza dietro le spalle
        float heightOffset   = 0.2f; // Altezza sopra la testa
        float sideOffset     = 0.6f; // Spostamento sulla spalla destra

        glm::vec3 camPos = position
                         - (forward * distanceBehind)
                         + (up * heightOffset)
                         + (right * sideOffset);

        glm::vec3 targetPos = position + (forward * 5.0f) + (up * 0.5f);

        return glm::lookAt(camPos, targetPos, up);
    }

    // 3. Default: Prima Persona
    return glm::lookAt(position, position + getForwardVector(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Player::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 Prj = glm::perspective(FOVy, aspectRatio, nearPlane, farPlane);
    Prj[1][1] *= -1; // Inversione asse Y per Vulkan
    return Prj;
}

glm::mat4 Player::getViewProjectionMatrix(float aspectRatio) const {
    return getProjectionMatrix(aspectRatio) * getViewMatrix();
}

glm::mat4 Player::getWorldMatrix() const {
    // 1. Applichiamo l'offset verticale (-2.9f per far coincidere la base del modello con il pavimento/collider)
    glm::vec3 meshPos = position;
    meshPos.y -= 2.9f;

    glm::mat4 customWorld = glm::translate(glm::mat4(1.0f), meshPos);

    // 2. Ruota verso lo Yaw di visuale
    customWorld = glm::rotate(customWorld, glm::radians(-yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // 3. Raddrizza il modello Mixamo (da sdraiato a in piedi)
    customWorld = glm::rotate(customWorld, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // 4. Applica la scala dal scene.json (0.018)
    customWorld = glm::scale(customWorld, glm::vec3(0.018f));

    return customWorld;
}