#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <fstream>
#include <json.hpp>
#include <cmath>
#include <algorithm>
#include <glm/gtx/norm.hpp>
#include <unordered_map>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "Player.hpp"
#include "PhysicsManager.hpp"

PhysicsManager::PhysicsManager() {
    gravity = 15.0f;
    floorCollider = nullptr;
    heldObjectIndex = -1;
    qPressedLastFrame = false;
    tPressedLastFrame = false;
    uiState = PhysicsUIState::NONE;
}

PhysicsManager::~PhysicsManager() {
    if (floorCollider != nullptr) {
        delete floorCollider;
        floorCollider = nullptr;
    }
    for (Collider *cld: customColliders) {
        delete cld;
    }
    customColliders.clear();
}

void PhysicsManager::init(Scene &scene, const std::string &sceneFilePath, float floorLevel) {
    // Inizializza il collider infinito per il pavimento
    if (floorCollider == nullptr) floorCollider = new Collider();
    floorCollider->initAABB(-50.0f, -1.0f, -50.0f, 50.0f, floorLevel, 50.0f);
    floorCollider->setWorldMatrix(glm::mat4(1.0f));
    physicsObjects.clear();

    try {
        std::ifstream ifs(sceneFilePath);
        if (ifs.is_open()) {
            nlohmann::json js;
            ifs >> js;
            ifs.close();

            if (js.contains("instances")) {
                // Parsing dei collider custom dal JSON (es. muri invisibili, banconi della taverna)
                if (js.contains("customColliders")) {
                    for (const auto &cc: js["customColliders"]) {
                        std::string type = cc.value("type", "AABB");
                        if (type == "AABB" && cc.contains("params")) {
                            std::vector<float> p = cc["params"].get<std::vector<float> >();
                            if (p.size() >= 6) {
                                Collider *cld = new Collider();
                                cld->initAABB(p[0], p[1], p[2], p[3], p[4], p[5]);
                                cld->setWorldMatrix(glm::mat4(1.0f));
                                customColliders.push_back(cld);

                                // Debug visuale opzionale
                                if (cc.value("visible", false)) {
                                    scene.ColShow.show(cld);
                                }
                            }
                        }
                    }
                }

                // Estrazione parametri fisici (massa, rimbalzo, attrito) per gli oggetti interattivi
                for (const auto &tech: js["instances"]) {
                    for (const auto &el: tech["elements"]) {
                        if (el.value("physics", false)) {
                            std::string instId = el["id"].template get<std::string>();
                            auto it = scene.InstanceIds.find(instId);
                            if (it != scene.InstanceIds.end() && it->second < scene.InstanceCount && scene.I[it->second]
                                != nullptr) {
                                float m = el.value("mass", 1.5f);
                                float b = el.value("bounciness", 0.4f);
                                float f = el.value("friction", 0.85f);

                                Instance *inst = scene.I[it->second];

                                // Estrae la scala originale dalla matrice per non deformare l'oggetto durante le rotazioni
                                glm::vec3 oScale(
                                    glm::length(glm::vec3(inst->Wm[0])),
                                    glm::length(glm::vec3(inst->Wm[1])),
                                    glm::length(glm::vec3(inst->Wm[2]))
                                );
                                if (std::isnan(oScale.x) || oScale.x < 0.001f) oScale = glm::vec3(1.0f);

                                // Estrae la rotazione base tramite quaternioni
                                glm::mat3 rotMat(
                                    glm::vec3(inst->Wm[0]) / oScale.x,
                                    glm::vec3(inst->Wm[1]) / oScale.y,
                                    glm::vec3(inst->Wm[2]) / oScale.z
                                );
                                glm::quat oRot = glm::quat_cast(rotMat);

                                physicsObjects.push_back({
                                    it->second, glm::vec3(0.0f), glm::vec3(0.0f), m, b, f, false, oScale, oRot
                                });
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

void PhysicsManager::update(GLFWwindow *window, float deltaT, Scene &scene, const Player &player, bool canInteract,
                            TextMaker &txt) {
    // Cap al deltaT per evitare compenetrazioni (tunneling) in caso di lag
    if (deltaT > 0.05f) deltaT = 0.05f;
    PhysicsUIState targetUIState = PhysicsUIState::NONE;

    // Lettura input unificata: Tastiera / Gamepad per il controllo oggetti
    bool qPressedKey = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    bool tPressedKey = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;

    GLFWgamepadstate gamepadState;
    bool hasGamepad = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

    // Toggle presa
    bool grabPressed = qPressedKey || (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS);
    bool grabTriggered = grabPressed && !qPressedLastFrame;

    // Toggle lancio
    bool throwPressed = tPressedKey || (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] ==
                                        GLFW_PRESS);
    bool throwTriggered = throwPressed && !tPressedLastFrame;


    // --- RAYCASTING E GRABBING ---
    if (canInteract) {
        if (heldObjectIndex != -1) {
            targetUIState = PhysicsUIState::HOLD;

            if (grabTriggered) {
                // Rilascio sul posto
                physicsObjects[heldObjectIndex].isHeld = false;
                physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                heldObjectIndex = -1;
            } else if (throwTriggered) {
                // Lancio basato sulla massa e direzione della telecamera
                PhysicsObject &heldObj = physicsObjects[heldObjectIndex];
                heldObj.isHeld = false;

                float throwForce = 25.0f;
                heldObj.velocity = player.getForwardVector() * (throwForce / heldObj.mass) + glm::vec3(
                                       0.0f, 5.0f / heldObj.mass, 0.0f);

                // Aggiunta rotazione random per realismo visivo durante il volo
                float rx = ((rand() % 100) / 50.0f) - 1.0f;
                float ry = ((rand() % 100) / 50.0f) - 1.0f;
                float rz = ((rand() % 100) / 50.0f) - 1.0f;
                heldObj.angularVelocity = glm::vec3(rx, ry, rz) * (15.0f / heldObj.mass);

                heldObjectIndex = -1;
            }
        } else {
            // Raycasting: fa avanzare una sfera collider lungo il vettore forward
            // della telecamera per cercare oggetti interattivi a tiro
            int hitIndex = -1;
            Collider raycastPoint;
            raycastPoint.initSphere(0.0f, 0.0f, 0.0f, 0.15f);

            glm::vec3 rayOrigin = player.position;
            glm::vec3 rayDir = player.getForwardVector();

            for (float d = 0.5f; d <= 4.0f; d += 0.1f) {
                raycastPoint.setWorldMatrix(glm::translate(glm::mat4(1.0f), rayOrigin + rayDir * d));

                for (int i = 0; i < physicsObjects.size(); i++) {
                    Instance *inst = scene.I[physicsObjects[i].instanceIndex];
                    if (inst->C && raycastPoint.collidesWith(*(inst->C))) {
                        hitIndex = i;
                        break;
                    }
                }
                if (hitIndex != -1) break;

                // Ferma il raycast se becchiamo un muro invisibile
                bool hitWall = false;
                for (Collider *cld: customColliders) {
                    if (raycastPoint.collidesWith(*cld)) {
                        hitWall = true;
                        break;
                    }
                }
                if (hitWall) break;
            }

            if (hitIndex != -1) {
                targetUIState = PhysicsUIState::GRAB;
                if (grabTriggered) {
                    heldObjectIndex = hitIndex;
                    physicsObjects[heldObjectIndex].isHeld = true;
                    physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                    physicsObjects[heldObjectIndex].angularVelocity = glm::vec3(0.0f);
                    targetUIState = PhysicsUIState::HOLD;
                }
            }
        }
    }

    // Gestione UI contestuale (es. "Premi X per afferrare")
    bool textExists = (txt.Blocks.find(99) != txt.Blocks.end());

    if (targetUIState != uiState || (targetUIState != PhysicsUIState::NONE && !textExists)) {
        txt.removeText(99);
        std::string uiText = "";
        if (targetUIState == PhysicsUIState::GRAB) {
            hasGamepad ? uiText = "Premi X per afferrare" : uiText = "Premi Q per afferrare";
            txt.print(0.0f, 0.7f, uiText, 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER,
                      TRV_MIDDLE);
        } else if (targetUIState == PhysicsUIState::HOLD) {
            hasGamepad ? uiText = "X lascia | LB lancia" : uiText = "Q lascia | T lancia";
            txt.print(0.0f, 0.7f, uiText, 99, "SS", false, false, false, TAL_CENTER,
                      TRH_CENTER, TRV_MIDDLE);
        }
        uiState = targetUIState;
    }

    qPressedLastFrame = grabPressed;
    tPressedLastFrame = throwPressed;

    // --- CREAZIONE SPATIAL HASH GRID ---
    // Ottimizzazione Broad-Phase: mappiamo lo spazio 3D in una griglia 1D tramite un hash XOR.
    // Evita di testare ogni oggetto contro ogni altro oggetto.
    float cellSize = getCellSize();
    spatialGrid.clear();

    auto getCellID = [cellSize](glm::vec3 pos) -> int {
        int x = static_cast<int>(std::floor(pos.x / cellSize));
        int y = static_cast<int>(std::floor(pos.y / cellSize));
        int z = static_cast<int>(std::floor(pos.z / cellSize));
        // Numeri primi grandi per ridurre le collisioni di hash
        return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
    };

    // Popoliamo la griglia con le entità della scena per questo frame
    for (int i = 0; i < scene.InstanceCount; ++i) {
        Instance *inst = scene.I[i];
        if (inst && inst->C) {
            int cellID = getCellID(glm::vec3(inst->Wm[3]));
            spatialGrid[cellID].push_back(inst);
        }
    }

    // --- RISOLUZIONE FISICA ---
    for (auto &po: physicsObjects) {
        if (po.instanceIndex >= scene.InstanceCount) continue;
        Instance *inst = scene.I[po.instanceIndex];
        if (inst == nullptr || inst->C == nullptr) continue;

        glm::vec3 scale = po.originalScale;

        // Se l'oggetto è tenuto dal player, agganciamo la sua World Matrix davanti alla telecamera
        if (po.isHeld) {
            glm::vec3 holdPos = player.position + player.getForwardVector() * 2.0f;
            holdPos.y -= 0.3f;

            glm::mat4 playerYawMat = glm::rotate(glm::mat4(1.0f), glm::radians(player.yaw + 90.0f),
                                                 glm::vec3(0.0f, -1.0f, 0.0f));
            glm::mat4 originalRotMat = glm::mat4(po.originalRotation);

            inst->Wm = glm::translate(glm::mat4(1.0f), holdPos) * playerYawMat * originalRotMat * glm::scale(
                           glm::mat4(1.0f), scale);
            inst->C->setWorldMatrix(inst->Wm);
            continue;
        }

        // Applica rotazione angolare tramite Quaternioni per evitare il Gimbal Lock
        if (glm::length(po.angularVelocity) > 0.01f) {
            glm::vec3 pos = glm::vec3(inst->Wm[3]);

            glm::mat3 rotMat(
                glm::vec3(inst->Wm[0]) / scale.x,
                glm::vec3(inst->Wm[1]) / scale.y,
                glm::vec3(inst->Wm[2]) / scale.z
            );
            glm::quat q = glm::quat_cast(rotMat);

            glm::quat deltaQ = glm::angleAxis(glm::length(po.angularVelocity) * deltaT,
                                              glm::normalize(po.angularVelocity));
            q = glm::normalize(deltaQ * q);

            inst->Wm = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(q) * glm::scale(glm::mat4(1.0f), scale);
        }

        // Closure per risolvere le collisioni sfruttando lo Spatial Hashing
        auto checkCollision = [&]() -> bool {
            if (!inst->C) return false;
            inst->C->setWorldMatrix(inst->Wm);

            if (floorCollider != nullptr && inst->C->collidesWith(*floorCollider)) return true;

            glm::vec3 posA = glm::vec3(inst->Wm[3]);
            float currentCellSize = getCellSize();

            int myCellX = static_cast<int>(std::floor(posA.x / currentCellSize));
            int myCellY = static_cast<int>(std::floor(posA.y / currentCellSize));
            int myCellZ = static_cast<int>(std::floor(posA.z / currentCellSize));

            // Test collisione limitato alla cella dell'oggetto e alle 26 celle adiacenti
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        int neighborID = ((myCellX + dx) * 73856093) ^ ((myCellY + dy) * 19349663) ^ (
                                             (myCellZ + dz) * 83492791);

                        if (spatialGrid.find(neighborID) != spatialGrid.end()) {
                            for (Instance *otherInst: spatialGrid[neighborID]) {
                                if (otherInst == inst) continue;

                                glm::vec3 posB = glm::vec3(otherInst->Wm[3]);
                                float distSq = glm::distance2(posA, posB);

                                // Check rapido sulla distanza al quadrato prima dell'AABB vero e proprio
                                if (distSq > 25.0f) continue;
                                if (inst->C->collidesWith(*(otherInst->C))) return true;
                            }
                        }
                    }
                }
            }

            // Test contro ostacoli statici e player
            for (Collider *cld: customColliders) {
                if (cld && inst->C->collidesWith(*cld)) return true;
            }
            if (player.playerCollider && inst->C->collidesWith(*(player.playerCollider))) return true;

            return false;
        };

        glm::vec3 oldPos = glm::vec3(inst->Wm[3]);
        glm::vec3 newPos = oldPos;

        // --- INTEGRAZIONE MOVIMENTO (Asse Y) ---
        po.velocity.y -= gravity * deltaT;
        if (po.velocity.y < -25.0f) po.velocity.y = -25.0f; // Terminal velocity

        newPos.y += po.velocity.y * deltaT;
        inst->Wm[3][1] = newPos.y;

        if (checkCollision()) {
            newPos.y = oldPos.y; // Rollback
            inst->Wm[3][1] = newPos.y;

            po.velocity.y *= -po.bounciness;
            if (std::abs(po.velocity.y) < 1.0f) po.velocity.y = 0.0f;

            // Applica l'attrito orizzontale e rotazionale pesato sul deltaT per consistenza tra framerate
            float frictionFactor = std::pow(po.friction, deltaT * 60.0f);
            po.velocity.x *= frictionFactor;
            po.velocity.z *= frictionFactor;
            po.angularVelocity *= frictionFactor;

            // Stop dead per micro-velocità (previene il tremolio continuo a terra)
            if (glm::length(glm::vec2(po.velocity.x, po.velocity.z)) < 0.05f) {
                po.velocity.x = 0.0f;
                po.velocity.z = 0.0f;
            }

            // "Flattening": usa l'interpolazione sferica tra quaternioni per raddrizzare
            // fluidamente l'oggetto verso il suo orientamento originale di riposo quando quasi fermo.
            if (po.velocity.y == 0.0f && glm::length(po.velocity) < 0.5f && glm::length(po.angularVelocity) < 1.0f) {
                glm::mat3 rotMat(
                    glm::vec3(inst->Wm[0]) / scale.x,
                    glm::vec3(inst->Wm[1]) / scale.y,
                    glm::vec3(inst->Wm[2]) / scale.z
                );

                glm::quat q = glm::quat_cast(rotMat);
                q = glm::normalize(glm::slerp(q, po.originalRotation, 5.0f * deltaT));

                inst->Wm = glm::translate(glm::mat4(1.0f), newPos) * glm::mat4(q) * glm::scale(glm::mat4(1.0f), scale);

                if (glm::length(po.angularVelocity) < 0.1f) {
                    po.angularVelocity = glm::vec3(0.0f);
                }
            }
        }

        // --- INTEGRAZIONE MOVIMENTO (Asse X) ---
        newPos.x += po.velocity.x * deltaT;
        inst->Wm[3][0] = newPos.x;
        if (checkCollision()) {
            newPos.x = oldPos.x;
            inst->Wm[3][0] = newPos.x;
            po.velocity.x *= -po.bounciness;
        }

        // --- INTEGRAZIONE MOVIMENTO (Asse Z) ---
        newPos.z += po.velocity.z * deltaT;
        inst->Wm[3][2] = newPos.z;
        if (checkCollision()) {
            newPos.z = oldPos.z;
            inst->Wm[3][2] = newPos.z;
            po.velocity.z *= -po.bounciness;
        }

        inst->C->setWorldMatrix(inst->Wm);
    }
}

Collider *PhysicsManager::getFloorCollider() const { return floorCollider; }
const std::vector<Collider *> &PhysicsManager::getCustomColliders() const { return customColliders; }

int PhysicsManager::getHeldInstanceIndex() const {
    if (heldObjectIndex != -1 && static_cast<size_t>(heldObjectIndex) < physicsObjects.size()) {
        return physicsObjects[heldObjectIndex].instanceIndex;
    }
    return -1;
}

// Spawna e applica istantaneamente una forza ad un oggetto per il lancio da script
void PhysicsManager::throwObject(Scene &scene, const std::string &instanceName, glm::vec3 startPos,
                                 glm::vec3 velocity) {
    auto it = scene.InstanceIds.find(instanceName);
    if (it != scene.InstanceIds.end() && it->second < scene.InstanceCount && scene.I[it->second] != nullptr) {
        for (auto &po: physicsObjects) {
            if (po.instanceIndex == it->second) {
                scene.I[po.instanceIndex]->Wm[3] = glm::vec4(startPos, 1.0f);
                po.velocity = velocity;
                po.angularVelocity = glm::vec3(15.0f, 5.0f, 10.0f);
                po.isHeld = false;
                break;
            }
        }
    }
}
