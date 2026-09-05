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
    rPressedLastFrame = false;
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

    int heldObj = physManager.getHeldInstanceIndex();

    // --- NUOVA LOGICA: PARTIZIONAMENTO SPAZIALE ---

    // NOTA: Devi avere accesso a cellSize (es. 5.0f) e spatialGrid dal physManager.
    // Ipotizziamo che tu le abbia esposte.
    float cellSize = physManager.getCellSize();
    const auto& grid = physManager.getSpatialGrid();

    // Calcoliamo in quale cella si trova il testPos del player
    int myCellX = static_cast<int>(std::floor(testPos.x / cellSize));
    int myCellY = static_cast<int>(std::floor(testPos.y / cellSize));
    int myCellZ = static_cast<int>(std::floor(testPos.z / cellSize));

    // Controlliamo SOLO le 27 celle (la nostra + 26 vicine)
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                // Calcolo hash identico a quello nel PhysicsManager
                int neighborID = ((myCellX + dx) * 73856093) ^ ((myCellY + dy) * 19349663) ^ ((myCellZ + dz) * 83492791);

                auto it = grid.find(neighborID);
                if (it != grid.end()) {
                    for (Instance* inst : it->second) {

                        // Ignoriamo l'oggetto attualmente in mano (non possiamo collidere con lui)
                        // NOTA: Qui presuppongo tu possa ottenere l'indice dell'istanza o confrontare il puntatore
                        if (heldObj != -1 && scene.I[heldObj] == inst) continue;

                        // Ignoriamo la mesh del player stesso (come facevi nel tuo codice originale)
                        if (inst->id != nullptr) {
                            std::string objId = *(inst->id);
                            if (objId.find("player") != std::string::npos || objId.find("Player") != std::string::npos) {
                                continue;
                            }
                        }

                        // TEST DI COLLISIONE EFFETTIVO
                        if (inst->C != nullptr && playerCollider->collidesWith(*(inst->C))) {
                            return true; // Trovata collisione!
                        }
                    }
                }
            }
        }
    }

    // 3. Test contro i muri custom invisibili del JSON (Rimane inalterato)
    for (Collider* cld : physManager.getCustomColliders()) {
        if (playerCollider->collidesWith(*cld)) return true;
    }

    return false;
}

void Player::processInput(GLFWwindow* window, float deltaTime, const Scene& scene, const PhysicsManager& physManager) {
    // --- CONTROLLO INPUT DA GAMEPAD / JOYSTICK ---
    GLFWgamepadstate gamepadState;
    bool hasGamepad = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

    // Filter Deadzone per Stick Analogici
    auto applyDeadzone = [](float value, float threshold = 0.15f) -> float {
        if (std::abs(value) < threshold) return 0.0f;
        return value;
    };

    // --- TOGGLE FULLSCREEN (TASTO R o BACK/SELECT) ---
    bool rPressed = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS);

    if (rPressed && !rPressedLastFrame) {
        static int savedX = 100, savedY = 100, savedWidth = 1280, savedHeight = 720;

        bool isFullscreen = (glfwGetWindowMonitor(window) != nullptr);
        if (!isFullscreen) {
            glfwGetWindowPos(window, &savedX, &savedY);
            glfwGetWindowSize(window, &savedWidth, &savedHeight);

            GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

            glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, nullptr, savedX, savedY, savedWidth, savedHeight, GLFW_DONT_CARE);
        }
    }
    rPressedLastFrame = rPressed;

    // --- LIMITI TAVERNA ---
    const float minX = 1.11968f;
    const float maxX = 19.5197f;
    const float minZ = -5.76789f;
    const float maxZ = 21.0792f;
    const float minY = -1.0f;
    const float maxY = 16.0f;

    bool isInTavernRoom = (position.x >= minX && position.x <= maxX) &&
                          (position.y >= minY && position.y <= maxY) &&
                          (position.z >= minZ && position.z <= maxZ);

    // --- TOGGLE TELECAMERA FISSA TAVERNA (TASTO C o D-PAD DESTRA) ---
    bool cPressed = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS);

    if (cPressed && !cPressedLastFrame) {
        if (isInTavernRoom || isFixedCamera()) {
            cameraMode = (cameraMode + 1) % 3;
            if (cameraMode != 0) {
                isThirdPerson = false;
            }
        }
    }
    cPressedLastFrame = cPressed;

    // --- TOGGLE TERZA PERSONA (TASTO X o D-PAD SINISTRA) ---
    bool xPressed = (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS);

    if (xPressed && !xPressedLastFrame) {
        isThirdPerson = !isThirdPerson;
        if (isThirdPerson) {
            cameraMode = 0;
        }
    }
    xPressedLastFrame = xPressed;

    // --- ROTAZIONE VISUALE (MOUSE o STICK DESTRO GAMEPAD) ---
    updateMouseLook(window);
    if (hasGamepad) {
        float rightStickX = applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
        float rightStickY = applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);

        float gamepadRotSensitivity = 120.0f * deltaTime;
        yaw += rightStickX * gamepadRotSensitivity;
        pitch -= rightStickY * gamepadRotSensitivity; // Invertito per feeling naturale

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    if (deltaTime > 0.1f) deltaTime = 0.1f;

    // --- TOGGLE FLY MODE (TASTO M o TASTO Y / TRIANGOLO) ---
    bool mPressed = (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS);

    if (mPressed && !mPressedLastFrame) {
        flyMode = !flyMode;
        if (flyMode) playerVelocityY = 0.0f;
    }
    mPressedLastFrame = mPressed;

    // --- CORSA (SHIFT o GRILLETTO DESTRO / L3) ---
    bool isRunningInput = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                          (hasGamepad && (gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f ||
                                          gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS));

    float velocity = moveSpeed * deltaTime * (isRunningInput ? 1.8f : 1.0f);
    glm::vec3 forward = getForwardVector();
    glm::vec3 forwardFlatRaw(forward.x, 0.0f, forward.z);
    glm::vec3 forwardFlat = (glm::length(forwardFlatRaw) > 0.0001f)
                          ? glm::normalize(forwardFlatRaw)
                          : glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rightFlat = getRightVector();

    // --- MOVIMENTO WASD / STICK SINISTRO ---
    glm::vec3 desiredMove(0.0f);

    // Tastiera
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) desiredMove += forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) desiredMove -= forwardFlat;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) desiredMove -= rightFlat;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) desiredMove += rightFlat;

    // Gamepad Stick Sinistro
    if (hasGamepad) {
        float leftStickX = applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        float leftStickY = applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);

        desiredMove += rightFlat * leftStickX;
        desiredMove -= forwardFlat * leftStickY;
    }

    // --- LOGICA FLY MODE ---
    if (flyMode) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS ||
           (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS)) {
            position.y += velocity * 2.0f;
        }
        if (glm::length(desiredMove) > 0.01f) {
            position += glm::normalize(desiredMove) * velocity;
        }

        playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
        return;
    }

    // Movimento con scomposizione dello Step
    if (glm::length(desiredMove) > 0.01f) {
        desiredMove = glm::normalize(desiredMove) * velocity;
    }

    glm::vec3 resolvedPos = position;
    float stepHeight = 0.6f;

    float moveLength = glm::length(desiredMove);
    int numSteps = static_cast<int>(moveLength / 0.1f) + 1;
    if (numSteps < 1) numSteps = 1;
    if (numSteps > 8) numSteps = 8;
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

    // Gravità e Salto (SPAZIO o TASTO A / CROCE)
    playerVelocityY -= 40.0f * deltaTime;

    glm::vec3 testY = resolvedPos;
    testY.y += playerVelocityY * deltaTime;

    bool jumpInput = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) ||
                     (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

    if (!checkCollisionAt(testY, scene, physManager)) {
        resolvedPos.y = testY.y;
    } else {
        if (playerVelocityY < 0.0f) {
            playerVelocityY = 0.0f;
            if (jumpInput) {
                playerVelocityY = 12.0f;
            }
        } else {
            playerVelocityY = 0.0f;
        }
    }

    // Limite radiale mondo su piano XZ
    const glm::vec2 worldCenterXZ(15.0f, -40.0f);
    const float worldRadius = 90.0f;
    glm::vec2 playerXZ(resolvedPos.x, resolvedPos.z);
    glm::vec2 deltaXZ = playerXZ - worldCenterXZ;
    float deltaLen = glm::length(deltaXZ);
    if (deltaLen > worldRadius && deltaLen > 0.0001f) {
        glm::vec2 clampedXZ = worldCenterXZ + (deltaXZ / deltaLen) * worldRadius;
        resolvedPos.x = clampedXZ.x;
        resolvedPos.z = clampedXZ.y;
    }

    // Aggiorna posizione finale e collider
    position = resolvedPos;
    playerCollider->setWorldMatrix(glm::translate(glm::mat4(1.0f), position));
}

glm::mat4 Player::getViewMatrix() const {
    // 1. Telecamere fisse della Taverna (Tasto C / D-Pad Destra)
    if (cameraMode == 1) {
        return glm::lookAt(fixedCamPos1, fixedCamTarget1, glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (cameraMode == 2) {
        return glm::lookAt(fixedCamPos2, fixedCamTarget2, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // 2. Terza Persona stile Fortnite (Tasto X / D-Pad Sinistra)
    if (isThirdPerson) {
        glm::vec3 forward = getForwardVector();
        glm::vec3 right = getRightVector();
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        float distanceBehind = 4.5f;
        float heightOffset   = 0.2f;
        float sideOffset     = 0.6f;

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
    glm::vec3 meshPos = position;
    meshPos.y -= 2.9f;

    glm::mat4 customWorld = glm::translate(glm::mat4(1.0f), meshPos);
    customWorld = glm::rotate(customWorld, glm::radians(-yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    customWorld = glm::rotate(customWorld, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    customWorld = glm::scale(customWorld, glm::vec3(0.018f));

    return customWorld;
}