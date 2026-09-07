#include "MissionManager.hpp"
#include "DialogueManager.hpp"
#include "PhysicsManager.hpp"
#include "modules/TextMaker.hpp"
#include <glm/glm.hpp>
#include <cmath>

MissionManager::MissionManager() : currentMissionIndex(-1), totalItemsForCurrent(0), counterTextId(-1) {
}

void MissionManager::addCollectionMission(int reqState, const std::string &keyword, const std::string &destId,
                                          int nextState, const std::string &text, const std::string &succText,
                                          float rTol, float hTol) {
    missions.push_back({reqState, keyword, destId, nextState, text, succText, rTol, hTol});
}

void MissionManager::checkAndInitMission(int currentStoryProgress, Scene &SC) {
    // Evita di ricaricare tutto se stiamo già tracciando la missione di questa fase
    if (currentMissionIndex != -1 && missions[currentMissionIndex].requiredStoryState == currentStoryProgress) {
        return;
    }

    // Se la storia è avanzata, facciamo reset per preparare la prossima (o per svuotare se non c'è nulla)
    currentMissionIndex = -1;
    currentItemIndices.clear();
    totalItemsForCurrent = 0;

    // Cerca tra le missioni registrate se ce n'è una triggerata dallo stato attuale
    for (size_t i = 0; i < missions.size(); ++i) {
        if (missions[i].requiredStoryState == currentStoryProgress) {
            currentMissionIndex = static_cast<int>(i);

            // Scansiona l'intera scena per trovare gli ID degli oggetti richiesti tramite keyword
            for (auto const &[name, idx]: SC.InstanceIds) {
                if (name.find(missions[i].targetItemKeyword) != std::string::npos) {
                    currentItemIndices.push_back(idx);
                }
            }
            totalItemsForCurrent = currentItemIndices.size();
            break;
        }
    }
}

void MissionManager::update(float deltaT, DialogueManager &dialogueManager, Scene &SC,
                            const PhysicsManager &physicsManager, TextMaker &txt) {
    // Gestione a tempo per il popup "Missione Completata"
    if (successTimer > 0.0f) {
        successTimer -= deltaT;
        successTextId = txt.print(0.9f, -0.8f, currentSuccessText, successTextId, "SS", false, false, false, TAL_RIGHT,
                                  TRH_RIGHT, TRV_TOP, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        if (successTimer <= 0.0f && successTextId != -1) {
            txt.removeText(successTextId);
            successTextId = -1;
        }
    }

    // La progressione è legata a filo doppio con i dialoghi
    int progress = dialogueManager.getStoryProgress();

    checkAndInitMission(progress, SC);

    if (currentMissionIndex != -1) {
        const auto &mission = missions[currentMissionIndex];
        int itemsOnTarget = 0;

        // Recupera le coordinate fisiche della drop-zone
        auto itDest = SC.InstanceIds.find(mission.targetDestinationId);
        if (itDest != SC.InstanceIds.end()) {
            glm::vec3 posDest = glm::vec3(SC.I[itDest->second]->Wm[3]);
            int heldIdx = physicsManager.getHeldInstanceIndex();

            // Controllo distanze per capire quanti oggetti sono effettivamente a destinazione
            for (int idx: currentItemIndices) {
                // Se il player lo ha ancora in mano, non considerarlo consegnato
                if (idx == heldIdx) continue;

                glm::vec3 posItem = glm::vec3(SC.I[idx]->Wm[3]);
                glm::vec2 posDestXZ(posDest.x, posDest.z);
                glm::vec2 posItemXZ(posItem.x, posItem.z);

                // Check cilindrico: misuriamo la distanza orizzontale (XZ) e tolleriamo una differenza d'altezza (Y)
                float distXZ = glm::length(posDestXZ - posItemXZ);
                float dy = std::abs(posItem.y - posDest.y);

                if (distXZ <= mission.radiusTolerance && dy <= mission.heightTolerance) {
                    itemsOnTarget++;
                }
            }
        }

        // Render contatore UI in alto a destra
        std::string counterStr = mission.uiText + ": " + std::to_string(itemsOnTarget) + "/" + std::to_string(
                                     totalItemsForCurrent);
        counterTextId = txt.print(0.9f, -0.9f, counterStr, counterTextId, "SS", false, false, false, TAL_RIGHT,
                                  TRH_RIGHT, TRV_TOP, glm::vec4(1.0f, 0.8f, 0.0f, 1.0f), glm::vec4(0),
                                  glm::vec4(0, 0, 0, 0.8f), 1.0f, 1.0f);

        // Check condizione di vittoria
        if (itemsOnTarget >= totalItemsForCurrent && totalItemsForCurrent > 0) {
            // Sblocca lo step successivo della storia
            dialogueManager.setStoryProgress(mission.nextStoryState);

            currentSuccessText = mission.successText;
            successTimer = 10.0f; // Popup visibile per 10 secondi

            // Pulisce la UI del contatore e resetta le variabili per la prossima quest
            if (counterTextId != -1) {
                txt.removeText(counterTextId);
                counterTextId = -1;
            }
            currentMissionIndex = -1;
            currentItemIndices.clear();
        }
    } else {
        // Fallback di sicurezza: se non ci sono missioni attive, pulisci eventuali contatori HUD rimasti appesi
        if (counterTextId != -1) {
            txt.removeText(counterTextId);
            counterTextId = -1;
        }
    }
}

void MissionManager::cleanup(TextMaker &txt) {
    if (counterTextId != -1) {
        txt.removeText(counterTextId);
        counterTextId = -1;
    }
}
