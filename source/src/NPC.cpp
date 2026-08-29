#include "NPC.hpp"

void TavernNPC::update(float deltaT, bool isTalking, const glm::vec3& playerPos, AnimatedNPCRig& animManager, Scene& SC) {
    bool updateMatrix = false;

    if (isTalking) {
        // --- L'NPC STA PARLANDO CON IL GIOCATORE ---
        hasInteracted = true;
        isWaiting = false; // Interrompe un'eventuale attesa per parlare

        // Scegli animazione: Se ne ha > 1 usa l'indice 1 (Talk), altrimenti 0 (Idle)
        int animToPlay = (numAnimations > 1) ? 1 : 0;
        if (numAnimations > 0 && currentAnim != animToPlay) {
            animManager.play(name, animToPlay, 0.2f);
            currentAnim = animToPlay;
        }

        // Ruota verso il giocatore
        glm::vec3 dir = playerPos - position;
        dir.y = 0.0f;
        if (glm::length(dir) > 0.001f) {
            currentYaw = atan2(dir.x, dir.z);
        }
        updateMatrix = true;

    } else {
        // --- L'NPC E' LIBERO ---
        if (!waypoints.empty()) {
            hasInteracted = true;

            if (isWaiting) {
                // Sta aspettando a un waypoint
                currentWaitTimer -= deltaT;

                // Se ha una 3° animazione (indice 2), la usa, altrimenti usa la 0
                int waitAnim = (numAnimations > 2) ? 2 : 0;
                if (numAnimations > 0 && currentAnim != waitAnim) {
                    animManager.play(name, waitAnim, 0.2f);
                    currentAnim = waitAnim;
                }

                if (currentWaitTimer <= 0.0f) {
                    isWaiting = false;
                    currentWaypoint = (currentWaypoint + 1) % waypoints.size();
                }
                updateMatrix = true;
            } else {
                // E' in movimento
                if (numAnimations > 0 && currentAnim != 0) {
                    animManager.play(name, 0, 0.2f); // Animazione 0 (Walk)
                    currentAnim = 0;
                }

                glm::vec3 target = waypoints[currentWaypoint];
                glm::vec3 dir = target - position;
                float dist = glm::length(dir);

                if (dist < 0.2f) {
                    // Waypoint raggiunto! Controlla se deve aspettare
                    float wTime = (waitTimes.size() > currentWaypoint) ? waitTimes[currentWaypoint] : 0.0f;
                    if (wTime > 0.0f) {
                        isWaiting = true;
                        currentWaitTimer = wTime;
                    } else {
                        // Se non c'è attesa, passa subito al prossimo
                        currentWaypoint = (currentWaypoint + 1) % waypoints.size();
                    }
                } else {
                    // Muovi l'NPC
                    glm::vec3 moveDir = glm::normalize(dir);
                    position += moveDir * speed * deltaT;
                    currentYaw = atan2(moveDir.x, moveDir.z);
                }
                updateMatrix = true;
            }
        } else if (hasInteracted) {
            // NPC statico che ha finito di parlare
            if (numAnimations > 0 && currentAnim != 0) {
                animManager.play(name, 0, 0.2f);
                currentAnim = 0;
            }
            updateMatrix = true;
        }
    }

    // --- AGGIORNA MATRICE SCENA ---
    if (updateMatrix) {
        auto it = SC.InstanceIds.find(name);
        if (it != SC.InstanceIds.end()) {
            Instance* inst = SC.I[it->second];
            inst->Wm = glm::translate(glm::mat4(1.0f), position) *
                       glm::rotate(glm::mat4(1.0f), currentYaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                       glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                       glm::scale(glm::mat4(1.0f), scale);
        }
    }
}