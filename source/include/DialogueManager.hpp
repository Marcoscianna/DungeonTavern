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
    // Avanzamento globale della trama (utile per sbloccare nuovi dialoghi o missioni)
    int globalStoryProgress;

    // Gestione del focus sull'NPC più vicino
    bool showInteractionPrompt;
    int activeNPC;
    int lastActiveNPC;

    // Flag e indici per la conversazione correntemente attiva
    bool inDialogue;
    int dialogueNPC;
    int dialogueIndex;

    // Handle della UI per aggiornare o rimuovere le stringhe a schermo
    int dialogueTextId;
    int interactionPromptTextId;

    // Contatori per l'effetto "typewriter" (rivelazione progressiva dei caratteri)
    float dialogueRevealCount;
    float dialogueRevealSpeed;

    // Sistema anti-rimbalzo (debounce) per evitare doppi input indesiderati
    bool debounce;
    int curDebounce;

    // Navigazione nei dialoghi ad albero
    int currentTreeNodeId;
    int selectedChoiceIndex;

    // Utility per il word-wrapping dinamico in modo da non uscire dai bordi dello schermo
    std::string wrapText(const std::string &text, int maxLineLen);

    // Riferimenti ai dati del dialogo in corso (variano in base allo stato della storia o al tipo di NPC)
    InteractionType activeType;
    const std::vector<std::string> *activeDialogues{};
    const std::map<int, DialogueNode> *activeTree{};

public:
    DialogueManager();

    // Loop principale: controlla distanze NPC, gestisce input utente e aggiorna render testo
    void update(GLFWwindow *window, float deltaT, Player &player,
                const std::vector<TavernNPC> &npcs, TextMaker &txt, int windowWidth);

    // Blocca l'input di movimento del player all'esterno della classe
    bool isDialogueActive() const;

    // Pulizia finale per non lasciare stringhe UI appese alla chiusura/cambio stato
    void cleanup(TextMaker &txt);

    int getDialogueNPC() const;

    int getStoryProgress() const { return globalStoryProgress; }
    void setStoryProgress(int progress) { globalStoryProgress = progress; }

    // Avvia un dialogo bypassando il check di distanza
    void forceStartDialogue(const std::string &npcName, const std::vector<TavernNPC> &npcs, TextMaker &txt,
                            Player &player);
};
#endif //SKELETONTOCHANGE_DIALOGUEMANAGER_HPP
