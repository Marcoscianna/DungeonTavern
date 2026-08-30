#ifndef MISSION_MANAGER_HPP
#define MISSION_MANAGER_HPP

#include <vector>
#include <string>

// Forward declarations per evitare dipendenze circolari
class Scene;
class PhysicsManager;
class TextMaker;
class DialogueManager;

// Struttura che definisce una singola missione di raccolta parametrica
struct CollectionMission {
    int requiredStoryState;
    std::string targetItemKeyword;
    std::string targetDestinationId;
    int nextStoryState;
    std::string uiText;
    float radiusTolerance;
    float heightTolerance;
};

class MissionManager {
private:
    std::vector<CollectionMission> missions;
    int currentMissionIndex;
    std::vector<int> currentItemIndices;
    int totalItemsForCurrent;
    int counterTextId;

    // Cerca e inizializza una missione se il progresso della storia coincide
    void checkAndInitMission(int currentStoryProgress, Scene& SC);

public:
    MissionManager();

    // Aggiunge una missione alla lista del manager
    void addCollectionMission(int reqState, const std::string& keyword, const std::string& destId,
                              int nextState, const std::string& text, float rTol = 2.0f, float hTol = 2.0f);

    // Aggiorna lo stato delle missioni e la UI
    void update(DialogueManager& dialogueManager, Scene& SC, const PhysicsManager& physicsManager, TextMaker& txt);

    // Pulisce l'UI in uscita
    void cleanup(TextMaker& txt);
};

#endif // MISSION_MANAGER_HPP