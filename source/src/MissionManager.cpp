#include "MissionManager.hpp"
#include "DialogueManager.hpp"
#include "PhysicsManager.hpp"
#include "modules/TextMaker.hpp"
#include <glm/glm.hpp>
#include <cmath>

MissionManager::MissionManager() : currentMissionIndex(-1), totalItemsForCurrent(0), counterTextId(-1) {}

void MissionManager::addCollectionMission(int reqState, const std::string& keyword, const std::string& destId,
                                          int nextState, const std::string& text, const std::string& succText, float rTol, float hTol) {
    missions.push_back({reqState, keyword, destId, nextState, text, succText, rTol, hTol});
}

void MissionManager::checkAndInitMission(int currentStoryProgress, Scene& SC) {
    // Se la missione attiva coincide con il progresso della storia, è già inizializzata
    if (currentMissionIndex != -1 && missions[currentMissionIndex].requiredStoryState == currentStoryProgress) {
        return;
    }

    // Altrimenti resetta lo stato (la storia è cambiata o non c'è missione)
    currentMissionIndex = -1;
    currentItemIndices.clear();
    totalItemsForCurrent = 0;

    // Cerca se esiste una missione per lo stato attuale
    for (size_t i = 0; i < missions.size(); ++i) {
        if (missions[i].requiredStoryState == currentStoryProgress) {
            currentMissionIndex = static_cast<int>(i);

            // Trova tutti gli oggetti in scena che matchano la parola chiave
            for (auto const& [name, idx] : SC.InstanceIds) {
                if (name.find(missions[i].targetItemKeyword) != std::string::npos) {
                    currentItemIndices.push_back(idx);
                }
            }
            totalItemsForCurrent = currentItemIndices.size();
            break;
        }
    }
}

void MissionManager::update(float deltaT, DialogueManager& dialogueManager, Scene& SC, const PhysicsManager& physicsManager, TextMaker& txt) {
    // --- GESTIONE TIMER SUCCESSO ---
    if (successTimer > 0.0f) {
        successTimer -= deltaT;
        successTextId = txt.print(0.9f, -0.8f, currentSuccessText, successTextId, "SS", false, false, false, TAL_RIGHT, TRH_RIGHT, TRV_TOP, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        if (successTimer <= 0.0f && successTextId != -1) {
            txt.removeText(successTextId);
            successTextId = -1;
        }
    }
    // -------------------------------

    int progress = dialogueManager.getStoryProgress();

    // Inizializza o aggiorna la missione in corso
    checkAndInitMission(progress, SC);

    if (currentMissionIndex != -1) {
        const auto& mission = missions[currentMissionIndex];
        int itemsOnTarget = 0;

        // Trova la destinazione
        auto itDest = SC.InstanceIds.find(mission.targetDestinationId);
        if (itDest != SC.InstanceIds.end()) {
            glm::vec3 posDest = glm::vec3(SC.I[itDest->second]->Wm[3]);
            int heldIdx = physicsManager.getHeldInstanceIndex(); // Quale oggetto è in mano

            // Calcola quanti oggetti sono nell'area bersaglio
            for (int idx : currentItemIndices) {
                if (idx == heldIdx) continue; // Se lo stiamo tenendo in mano non conta

                glm::vec3 posItem = glm::vec3(SC.I[idx]->Wm[3]);
                glm::vec2 posDestXZ(posDest.x, posDest.z);
                glm::vec2 posItemXZ(posItem.x, posItem.z);

                float distXZ = glm::length(posDestXZ - posItemXZ);
                float dy = std::abs(posItem.y - posDest.y);

                if (distXZ <= mission.radiusTolerance && dy <= mission.heightTolerance) {
                    itemsOnTarget++;
                }
            }
        }

        // 3. UI Update in alto a destra
        std::string counterStr = mission.uiText + ": " + std::to_string(itemsOnTarget) + "/" + std::to_string(totalItemsForCurrent);
        counterTextId = txt.print(0.9f, -0.9f, counterStr, counterTextId, "SS", false, false, false, TAL_RIGHT, TRH_RIGHT, TRV_TOP, glm::vec4(1.0f, 0.8f, 0.0f, 1.0f), glm::vec4(0), glm::vec4(0,0,0,0.8f), 1.0f, 1.0f);

        if (itemsOnTarget >= totalItemsForCurrent && totalItemsForCurrent > 0) {
            dialogueManager.setStoryProgress(mission.nextStoryState);

            // Innesca il testo di successo
            currentSuccessText = mission.successText;
            successTimer = 10.0f; // Mostra il testo per 10 secondi

            if (counterTextId != -1) { txt.removeText(counterTextId); counterTextId = -1; }
            currentMissionIndex = -1;
            currentItemIndices.clear();
        }
    } else {
        // Nessuna missione attiva, assicurati che la UI sia pulita
        if (counterTextId != -1) {
            txt.removeText(counterTextId);
            counterTextId = -1;
        }
    }
}

void MissionManager::cleanup(TextMaker& txt) {
    if (counterTextId != -1) {
        txt.removeText(counterTextId);
        counterTextId = -1;
    }
}