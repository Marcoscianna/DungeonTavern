#include <iostream>
#include <fstream>
#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "Player.hpp"
#include "PhysicsManager.hpp"

PhysicsManager::PhysicsManager() {
    gravity = 9.81f;
    floorCollider = nullptr;
    heldObjectIndex = -1;
    ePressedLastFrame = false;
    tPressedLastFrame = false;
    uiState = PhysicsUIState::NONE;
}

PhysicsManager::~PhysicsManager() {
    if (floorCollider != nullptr) {
        delete floorCollider;
        floorCollider = nullptr;
    }
}
void PhysicsManager::init(const Scene& scene, const std::string& sceneFilePath, float floorLevel) {
    if (floorCollider == nullptr) floorCollider = new Collider();
    floorCollider->initAABB(-50.0f, -1.0f, -50.0f, 50.0f, floorLevel, 50.0f); //[cite: 2]

    physicsObjects.clear();
    try {
        std::ifstream ifs(sceneFilePath);
        if (ifs.is_open()) {
            nlohmann::json js;
            ifs >> js;
            ifs.close();
            if (js.contains("instances")) {
                for (const auto &tech: js["instances"]) {
                    for (const auto &el: tech["elements"]) {
                        if (el.value("physics", false)) {
                            std::string instId = el["id"].template get<std::string>();
                            auto it = scene.InstanceIds.find(instId); //[cite: 3]
                            if (it != scene.InstanceIds.end()) {
                                physicsObjects.push_back({it->second, glm::vec3(0.0f), false});
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        std::cout << "Warning: could not parse " << sceneFilePath << " for physics\n";
    }
}

void PhysicsManager::update(GLFWwindow* window, float deltaT, Scene& scene, const Player& player, bool canInteract, TextMaker& txt) {
    bool ePressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool tPressed = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;

    PhysicsUIState targetUIState = PhysicsUIState::NONE;

    // --- LOGICA DI INPUT E RICERCA OGGETTO ---
    if (canInteract) {
        if (heldObjectIndex != -1) {
            targetUIState = PhysicsUIState::HOLD;

            // Logica Drop e Throw
            if (ePressed && !ePressedLastFrame) {
                physicsObjects[heldObjectIndex].isHeld = false;
                physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                heldObjectIndex = -1;
            } else if (tPressed && !tPressedLastFrame) {
                physicsObjects[heldObjectIndex].isHeld = false;
                // Spinta per il lancio
                physicsObjects[heldObjectIndex].velocity = player.getForwardVector() * 12.0f + glm::vec3(0.0f, 3.0f, 0.0f);
                heldObjectIndex = -1;
            }
        } else {
            // Ricerca costante dell'oggetto più vicino da guardare
            float bestDist = 4.0f;
            int bestIdx = -1;
            for (int i = 0; i < physicsObjects.size(); i++) {
                Instance* inst = scene.I[physicsObjects[i].instanceIndex]; //[cite: 3]
                glm::vec3 objPos = glm::vec3(inst->Wm[3][0], inst->Wm[3][1], inst->Wm[3][2]);
                glm::vec3 dirToObj = objPos - player.position;
                float dist = glm::length(dirToObj);

                if (dist > 0.1f && dist < bestDist) {
                    glm::vec3 normDir = glm::normalize(dirToObj);
                    float dot = glm::dot(normDir, player.getForwardVector());
                    if (dot > 0.85f) { // Nel cono visivo del giocatore
                        bestDist = dist;
                        bestIdx = i;
                    }
                }
            }

            if (bestIdx != -1) {
                targetUIState = PhysicsUIState::GRAB;
                // Logica Grab
                if (ePressed && !ePressedLastFrame) {
                    heldObjectIndex = bestIdx;
                    physicsObjects[heldObjectIndex].isHeld = true;
                    physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                    targetUIState = PhysicsUIState::HOLD; // Aggiorna subito lo stato
                }
            }
        }
    }

    // --- AGGIORNAMENTO OTTIMIZZATO DELLA UI ---
    if (targetUIState != uiState) {
        if (targetUIState == PhysicsUIState::NONE) {
            txt.removeText(99); // Usiamo l'ID 99 per non intralciare i dialoghi[cite: 5]
        } else if (targetUIState == PhysicsUIState::GRAB) {
            txt.print(0.0f, 0.7f, "Premi E per afferrare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE); //[cite: 5]
        } else if (targetUIState == PhysicsUIState::HOLD) {
            txt.print(0.0f, 0.7f, "E per lasciare, T per lanciare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE); //[cite: 5]
        }
        uiState = targetUIState;
    }

    ePressedLastFrame = ePressed;
    tPressedLastFrame = tPressed;

    // --- LOGICA DELLA FISICA SPAZIALE ---
    for (auto& po : physicsObjects) {
        Instance* inst = scene.I[po.instanceIndex]; //[cite: 3]
        if (inst == nullptr || inst->C == nullptr) continue;

        if (po.isHeld) {
            glm::vec3 holdPos = player.position + player.getForwardVector() * 2.0f;
            holdPos.y -= 0.3f;

            inst->Wm[3][0] = holdPos.x;
            inst->Wm[3][1] = holdPos.y;
            inst->Wm[3][2] = holdPos.z;
            inst->C->setWorldMatrix(inst->Wm); //[cite: 2]
            continue;
        }

        glm::vec3 oldPos = glm::vec3(inst->Wm[3][0], inst->Wm[3][1], inst->Wm[3][2]);

        po.velocity.y -= gravity * deltaT;
        glm::vec3 newPos = oldPos + po.velocity * deltaT;

        inst->Wm[3][0] = newPos.x;
        inst->Wm[3][1] = newPos.y;
        inst->Wm[3][2] = newPos.z;
        inst->C->setWorldMatrix(inst->Wm); //[cite: 2]

        bool collided = false;
        for (int j = 0; j < scene.InstanceCount; j++) { //[cite: 3]
            if (j == po.instanceIndex) continue;
            if (*(scene.I[j]->id) == "house") continue;

            if (scene.I[j]->C != nullptr && inst->C->collidesWith(*(scene.I[j]->C))) { //[cite: 2, 3]
                collided = true;
                break;
            }
        }

        if (!collided && floorCollider != nullptr && inst->C->collidesWith(*floorCollider)) { //[cite: 2]
            collided = true;
        }

        if (!collided && player.getCollider() != nullptr && inst->C->collidesWith(*(player.getCollider()))) {
            collided = true;
        }

        if (collided) {
            po.velocity = glm::vec3(0.0f);
            inst->Wm[3][0] = oldPos.x;
            inst->Wm[3][1] = oldPos.y;
            inst->Wm[3][2] = oldPos.z;
            inst->C->setWorldMatrix(inst->Wm); //[cite: 2]
        }
    }
}

Collider* PhysicsManager::getFloorCollider() {
    return floorCollider;
}