#include <iostream>
#include <fstream>
#include <json.hpp>
#include <cmath>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "Player.hpp"
#include "PhysicsManager.hpp"

PhysicsManager::PhysicsManager() {
    gravity = 15.0f;
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
    for (Collider* cld : customColliders) {
        delete cld;
    }
    customColliders.clear();
}

void PhysicsManager::init(Scene& scene, const std::string& sceneFilePath, float floorLevel) {
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
                // --- LETTURA CUSTOM COLLIDERS ---
                if (js.contains("customColliders")) {
                    for (const auto& cc : js["customColliders"]) {
                        std::string type = cc.value("type", "AABB");
                        if (type == "AABB" && cc.contains("params")) {
                            std::vector<float> p = cc["params"].get<std::vector<float>>();
                            if (p.size() >= 6) {
                                Collider* cld = new Collider();
                                cld->initAABB(p[0], p[1], p[2], p[3], p[4], p[5]);
                                cld->setWorldMatrix(glm::mat4(1.0f));
                                customColliders.push_back(cld);

                                if (cc.value("visible", false)) {
                                    scene.ColShow.show(cld); // Disegna il wireframe per il debug
                                }
                            }
                        }
                    }
                }
                for (const auto &tech: js["instances"]) {
                    for (const auto &el: tech["elements"]) {
                        if (el.value("physics", false)) {
                            std::string instId = el["id"].template get<std::string>();
                            auto it = scene.InstanceIds.find(instId);
                            if (it != scene.InstanceIds.end()) {

                                float m = el.value("mass", 1.5f);
                                float b = el.value("bounciness", 0.4f);
                                float f = el.value("friction", 0.85f);

                                // ESTRAIAMO SCALA E ROTAZIONE ORIGINALE DEL MODELLO
                                Instance* inst = scene.I[it->second];
                                glm::vec3 oScale(glm::length(glm::vec3(inst->Wm[0])), glm::length(glm::vec3(inst->Wm[1])), glm::length(glm::vec3(inst->Wm[2])));
                                if (std::isnan(oScale.x) || oScale.x < 0.001f) oScale = glm::vec3(1.0f);

                                glm::mat3 rotMat(
                                    glm::vec3(inst->Wm[0]) / oScale.x,
                                    glm::vec3(inst->Wm[1]) / oScale.y,
                                    glm::vec3(inst->Wm[2]) / oScale.z
                                );
                                glm::quat oRot = glm::quat_cast(rotMat);

                                physicsObjects.push_back({it->second, glm::vec3(0.0f), glm::vec3(0.0f), m, b, f, false, oScale, oRot});
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

    // --- 1. RAYCASTING E GRABBING ---
    if (canInteract) {
        if (heldObjectIndex != -1) {
            targetUIState = PhysicsUIState::HOLD;

            if (ePressed && !ePressedLastFrame) {
                physicsObjects[heldObjectIndex].isHeld = false;
                physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                heldObjectIndex = -1;
            } else if (tPressed && !tPressedLastFrame) {
                PhysicsObject& heldObj = physicsObjects[heldObjectIndex];
                heldObj.isHeld = false;

                float throwForce = 25.0f;
                heldObj.velocity = player.getForwardVector() * (throwForce / heldObj.mass) + glm::vec3(0.0f, 5.0f / heldObj.mass, 0.0f);

                float rx = ((rand() % 100) / 50.0f) - 1.0f;
                float ry = ((rand() % 100) / 50.0f) - 1.0f;
                float rz = ((rand() % 100) / 50.0f) - 1.0f;
                heldObj.angularVelocity = glm::vec3(rx, ry, rz) * (15.0f / heldObj.mass);

                heldObjectIndex = -1;
            }
        } else {
            int hitIndex = -1;
            Collider raycastPoint;
            raycastPoint.initPoint(0,0,0);

            glm::vec3 rayOrigin = player.position;
            rayOrigin.y += 0.5f;
            glm::vec3 rayDir = player.getForwardVector();

            for (float d = 0.5f; d <= 4.0f; d += 0.2f) {
                raycastPoint.setWorldMatrix(glm::translate(glm::mat4(1.0f), rayOrigin + rayDir * d));

                // 1. Se il raggio colpisce un muro, si ferma (niente grab attraverso i muri)
                bool hitWall = false;
                for (Collider* cld : customColliders) {
                    if (raycastPoint.collidesWith(*cld)) {
                        hitWall = true;
                        break;
                    }
                }
                if (hitWall) break;

                // 2. Controlla gli oggetti
                for (int i = 0; i < physicsObjects.size(); i++) {
                    Instance* inst = scene.I[physicsObjects[i].instanceIndex];
                    if (inst->C && raycastPoint.collidesWith(*(inst->C))) {
                        hitIndex = i;
                        break;
                    }
                }
                if (hitIndex != -1) break;
            }

            if (hitIndex != -1) {
                targetUIState = PhysicsUIState::GRAB;
                if (ePressed && !ePressedLastFrame) {
                    heldObjectIndex = hitIndex;
                    physicsObjects[heldObjectIndex].isHeld = true;
                    physicsObjects[heldObjectIndex].velocity = glm::vec3(0.0f);
                    physicsObjects[heldObjectIndex].angularVelocity = glm::vec3(0.0f);
                    targetUIState = PhysicsUIState::HOLD;
                }
            }
        }
    }

    // --- AGGIORNAMENTO UI ---
    bool textExists = (txt.Blocks.find(99) != txt.Blocks.end());

    if (targetUIState != uiState) {
        txt.removeText(99);
        if (targetUIState == PhysicsUIState::GRAB) {
            txt.print(0.0f, 0.7f, "Premi E per afferrare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE);
        } else if (targetUIState == PhysicsUIState::HOLD) {
            txt.print(0.0f, 0.7f, "E per lasciare, T per lanciare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE);
        }
        uiState = targetUIState;
    } else if (targetUIState != PhysicsUIState::NONE && !textExists) {
        // Se il manager dei dialoghi cancella lo schermo, il Physics ripristina la sua scritta in sicurezza
        if (targetUIState == PhysicsUIState::GRAB) {
            txt.print(0.0f, 0.7f, "Premi E per afferrare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE);
        } else if (targetUIState == PhysicsUIState::HOLD) {
            txt.print(0.0f, 0.7f, "E per lasciare, T per lanciare", 99, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE);
        }
    }

    ePressedLastFrame = ePressed;
    tPressedLastFrame = tPressed;

    // --- 2. RISOLUZIONE FISICA ---
    for (auto& po : physicsObjects) {
        Instance* inst = scene.I[po.instanceIndex];
        if (inst == nullptr || inst->C == nullptr) continue;

        glm::vec3 scale = po.originalScale;

        // RIMETTE DRITTO L'OGGETTO USANDO LA SUA ROTAZIONE ORIGINALE
        if (po.isHeld) {
            glm::vec3 holdPos = player.position + player.getForwardVector() * 2.0f;
            holdPos.y -= 0.3f;

            // Calcoliamo la rotazione dello sguardo del giocatore
            glm::mat4 playerYawMat = glm::rotate(glm::mat4(1.0f), glm::radians(player.yaw + 90.0f), glm::vec3(0.0f, -1.0f, 0.0f));
            // Riprendiamo la rotazione originale nativa dell'oggetto dal JSON
            glm::mat4 originalRotMat = glm::mat4(po.originalRotation);

            // Combiniamo: Traslazione * (RotazionePlayer * RotazioneOriginale) * Scala
            inst->Wm = glm::translate(glm::mat4(1.0f), holdPos) * playerYawMat * originalRotMat * glm::scale(glm::mat4(1.0f), scale);
            inst->C->setWorldMatrix(inst->Wm);
            continue;
        }

        // Momento angolare
        if (glm::length(po.angularVelocity) > 0.01f) {
            glm::vec3 pos = glm::vec3(inst->Wm[3]);

            glm::mat3 rotMat(
                glm::vec3(inst->Wm[0]) / scale.x,
                glm::vec3(inst->Wm[1]) / scale.y,
                glm::vec3(inst->Wm[2]) / scale.z
            );
            glm::quat q = glm::quat_cast(rotMat);

            glm::quat deltaQ = glm::angleAxis(glm::length(po.angularVelocity) * deltaT, glm::normalize(po.angularVelocity));
            q = glm::normalize(deltaQ * q);

            inst->Wm = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(q) * glm::scale(glm::mat4(1.0f), scale);
        }

        auto checkCollision = [&]() -> bool {
            inst->C->setWorldMatrix(inst->Wm);

            float objectY = inst->Wm[3][1];
            float halfHeight = glm::length(glm::vec3(inst->Wm[1])) * 0.5f;

            if (objectY - halfHeight <= 0.0f) {
                return true;
            }

            // 2. Controllo Istanze
            for (int j = 0; j < scene.InstanceCount; j++) {
                if (j == po.instanceIndex) continue;
                if (scene.I[j]->C && inst->C->collidesWith(*(scene.I[j]->C))) return true;
            }

            // 3. Controllo Muri Invisibili JSON
            for (Collider* cld : customColliders) {
                if (inst->C->collidesWith(*cld)) return true;
            }

            // 4. Test contro la faccia del Giocatore
            if (player.getCollider() && inst->C->collidesWith(*(player.getCollider()))) return true; 

            return false;
        };

        glm::vec3 oldPos = glm::vec3(inst->Wm[3]);
        glm::vec3 newPos = oldPos;

        // Asse Y (Gravità e Rimbalzo a terra)
        po.velocity.y -= gravity * deltaT;
        newPos.y += po.velocity.y * deltaT;
        inst->Wm[3][1] = newPos.y;

        if (checkCollision()) {
            newPos.y = oldPos.y;
            inst->Wm[3][1] = newPos.y;

            po.velocity.y *= -po.bounciness;
            if (std::abs(po.velocity.y) < 1.5f) po.velocity.y = 0.0f;

            po.velocity.x *= po.friction;
            po.velocity.z *= po.friction;
            po.angularVelocity *= po.friction;

            // Allineamento naturale al suolo (Flattening)
            if (po.velocity.y == 0.0f && glm::length(po.velocity) < 1.0f && glm::length(po.angularVelocity) < 2.0f) {
                glm::mat3 rotMat(
                    glm::vec3(inst->Wm[0]) / scale.x,
                    glm::vec3(inst->Wm[1]) / scale.y,
                    glm::vec3(inst->Wm[2]) / scale.z
                );

                glm::quat q = glm::quat_cast(rotMat);
                glm::vec3 euler = glm::eulerAngles(q);

                const float pi2 = 1.5707963f;
                glm::vec3 targetEuler(
                    std::round(euler.x / pi2) * pi2,
                    euler.y,
                    std::round(euler.z / pi2) * pi2
                );

                glm::quat targetQ = glm::quat(targetEuler);
                q = glm::normalize(glm::slerp(q, targetQ, 10.0f * deltaT));

                inst->Wm = glm::translate(glm::mat4(1.0f), newPos) * glm::mat4(q) * glm::scale(glm::mat4(1.0f), scale);

                if (glm::length(po.angularVelocity) < 0.1f) {
                    po.angularVelocity = glm::vec3(0.0f);
                }
            }
        }

        // Asse X
        newPos.x += po.velocity.x * deltaT;
        inst->Wm[3][0] = newPos.x;
        if (checkCollision()) {
            newPos.x = oldPos.x;
            inst->Wm[3][0] = newPos.x;
            po.velocity.x *= -po.bounciness;
        }

        // Asse Z
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

Collider* PhysicsManager::getFloorCollider() const { return floorCollider; }
const std::vector<Collider*>& PhysicsManager::getCustomColliders() const { return customColliders; }
int PhysicsManager::getHeldInstanceIndex() const {
    if (heldObjectIndex != -1 && heldObjectIndex < physicsObjects.size()) {
        return physicsObjects[heldObjectIndex].instanceIndex;
    }
    return -1;
}