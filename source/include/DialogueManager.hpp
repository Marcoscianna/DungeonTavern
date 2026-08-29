#ifndef SKELETONTOCHANGE_DIALOGUEMANAGER_HPP
#define SKELETONTOCHANGE_DIALOGUEMANAGER_HPP
#pragma once

#include <vector>
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Player.hpp"
#include "NPC.hpp"

struct TextMaker;

class DialogueManager {
private:
    // Stato dell'interazione
    bool showInteractionPrompt;
    int activeNPC;
    int lastActiveNPC;

    // Stato del dialogo
    bool inDialogue;
    int dialogueNPC;
    int dialogueIndex;

    // ID dei testi per il TextMaker
    int dialogueTextId;
    int interactionPromptTextId;

    // Effetto Macchina da Scrivere
    float dialogueRevealCount;
    float dialogueRevealSpeed;

    // Input Debounce
    bool debounce;
    int curDebounce;

    // Variabili per dialghi
    int currentTreeNodeId;
    int selectedChoiceIndex;

    // Metodo helper interno per l'a capo automatico
    std::string wrapText(const std::string& text, int maxLineLen);

public:
    DialogueManager();

    // Metodo principale chiamato ogni frame
    void update(GLFWwindow* window, float deltaT, Player& player,
                const std::vector<TavernNPC>& npcs, TextMaker& txt, int windowWidth);

    // Getter per bloccare il movimento del player nel main
    bool isDialogueActive() const;

    // Pulizia
    void cleanup(TextMaker& txt);

    int getDialogueNPC() const;
};
#endif //SKELETONTOCHANGE_DIALOGUEMANAGER_HPP
