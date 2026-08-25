#include "DialogueManager.hpp"
#include "modules/TextMaker.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

DialogueManager::DialogueManager()
    : showInteractionPrompt(false), activeNPC(-1), lastActiveNPC(-1),
      inDialogue(false), dialogueNPC(-1), dialogueIndex(0),
      dialogueTextId(-1), interactionPromptTextId(-1),
      dialogueRevealCount(0.0f), dialogueRevealSpeed(45.0f),
      debounce(false), curDebounce(0),currentTreeNodeId(0), selectedChoiceIndex(0), navDebounceTimer(0.0f) {}

bool DialogueManager::isDialogueActive() const {
    return inDialogue;
}

std::string DialogueManager::wrapText(const std::string& text, int maxLineLen) {
    std::string result, currentLine, word;
    std::istringstream words(text);
    while (words >> word) {
        if (currentLine.length() + word.length() + 1 > maxLineLen) {
            if (!result.empty()) result += "\n";
            result += currentLine;
            currentLine = word;
        } else {
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        }
    }
    if (!currentLine.empty()) {
        if (!result.empty()) result += "\n";
        result += currentLine;
    }
    return result.empty() ? text : result;
}

void DialogueManager::update(GLFWwindow* window, float deltaT, Player& player,
                             const std::vector<TavernNPC>& npcs, TextMaker& txt, int windowWidth) {

    int maxChars = std::max(15, (int)(windowWidth / 22));

    // 1. Calcolo Distanze
    showInteractionPrompt = false;
    activeNPC = -1;
    float bestDistance = 99999.0f;

    for(size_t i = 0; i < npcs.size(); ++i) {
        glm::vec2 playerPosXZ(player.position.x, player.position.z);
        glm::vec2 npcPosXZ(npcs[i].position.x, npcs[i].position.z);
        float d = glm::length(playerPosXZ - npcPosXZ);
        float hDiff = std::abs(player.position.y - npcs[i].position.y);

        if(d < npcs[i].interactionRadius && hDiff <= 4.0f && d < bestDistance) {
            bestDistance = d;
            activeNPC = static_cast<int>(i);
            showInteractionPrompt = true;
        }
    }

    // 2. Gestione UI Prompt "Premi E"
    if(showInteractionPrompt && !inDialogue) {
        if(interactionPromptTextId == -1 || activeNPC != lastActiveNPC) {
            std::string p = wrapText(npcs[activeNPC].prompt, maxChars);
            interactionPromptTextId = txt.print(0.0f, -0.8f, p, interactionPromptTextId,
                                                "SS", false, false, false,
                                                TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.5f), 1.0f, 1.0f);
            lastActiveNPC = activeNPC;
        }
    } else {
        if(interactionPromptTextId != -1) {
            interactionPromptTextId = txt.print(0.0f, -0.8f, "", interactionPromptTextId,
                                                "SS", false, false, false,
                                                TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.5f), 1.0f, 1.0f);
            lastActiveNPC = -1;
        }
    }

    // 3. Input Dialogo e Navigazione
    if(showInteractionPrompt && glfwGetKey(window, GLFW_KEY_E) && !debounce) {
        debounce = true;
        curDebounce = 12;

        if(!inDialogue) {
            // Sistema anti-crash intelligente
            bool canStart = false;
            if (activeNPC >= 0) {
                if (npcs[activeNPC].type == InteractionType::BRANCHING && !npcs[activeNPC].dialogueTree.empty()) canStart = true;
                if (npcs[activeNPC].type != InteractionType::BRANCHING && !npcs[activeNPC].dialogues.empty()) canStart = true;
            }

            if(canStart) {
                inDialogue = true;
                dialogueNPC = activeNPC;
                dialogueIndex = 0;
                currentTreeNodeId = 0; // Inizia dalla radice dell'albero di Skyrim
                selectedChoiceIndex = 0;
                dialogueRevealCount = 0.0f;

                // Fix telecamera orizzontale
                glm::vec3 dir = npcs[dialogueNPC].position - player.position;
                float len = glm::length(glm::vec2(dir.x, dir.z));
                if(len > 0.001f) {
                    const float RAD2DEG = 180.0f / 3.14159265358979323846f;
                    player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
                }
                player.mouseLookInitialized = false;

                if (npcs[dialogueNPC].type == InteractionType::ONE_LINER) {
                    dialogueIndex = rand() % std::max((int)npcs[dialogueNPC].dialogues.size(), 1);
                }

                dialogueTextId = txt.print(0.0f, 0.6f, "", dialogueTextId, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
            }
        } else {
            // Logica Avanzamento Dialogo
            std::string currentFull = "";
            if (npcs[dialogueNPC].type == InteractionType::BRANCHING) {
                currentFull = npcs[dialogueNPC].dialogueTree.at(currentTreeNodeId).npcText;
            } else {
                currentFull = npcs[dialogueNPC].dialogues[dialogueIndex];
            }

            bool isRevealFinished = (dialogueRevealCount >= wrapText(currentFull, maxChars).length());

            if(!isRevealFinished) {
                // 2a. Salta effetto macchina da scrivere
                dialogueRevealCount = wrapText(currentFull, maxChars).length();
            } else {
                // 2b. Avanza logica
                if (npcs[dialogueNPC].type == InteractionType::ONE_LINER) {
                    inDialogue = false;
                }
                else if (npcs[dialogueNPC].type == InteractionType::LINEAR) {
                    ++dialogueIndex;
                    if(dialogueIndex < static_cast<int>(npcs[dialogueNPC].dialogues.size())) {
                        dialogueRevealCount = 0.0f;
                    } else {
                        inDialogue = false;
                    }
                }
                else if (npcs[dialogueNPC].type == InteractionType::BRANCHING) {
                    const auto& node = npcs[dialogueNPC].dialogueTree.at(currentTreeNodeId);
                    if (node.choices.empty()) {
                        inDialogue = false; // Il nodo non ha risposte: fine.
                    } else {
                        // Il giocatore ha scelto un'opzione!
                        int nextId = node.choices[selectedChoiceIndex].nextNodeId;
                        if (nextId == -1) {
                            inDialogue = false; // -1 significa esci
                        } else {
                            currentTreeNodeId = nextId;
                            selectedChoiceIndex = 0;
                            dialogueRevealCount = 0.0f;
                        }
                    }
                }

                // Chiudi in modo pulito
                if (!inDialogue) {
                    if(dialogueTextId != -1) dialogueTextId = txt.print(0.0f, 0.8f, "", dialogueTextId);
                    dialogueNPC = -1;
                    player.mouseLookInitialized = false;
                }
            }
        }
    }

    if(!glfwGetKey(window, GLFW_KEY_E)) debounce = false;
    if(curDebounce > 0) --curDebounce;

    // --- NAVIGAZIONE OPZIONI TIPO SKYRIM (W / S) ---
    if (navDebounceTimer > 0.0f) navDebounceTimer -= deltaT;
    if (inDialogue && dialogueNPC >= 0 && npcs[dialogueNPC].type == InteractionType::BRANCHING && navDebounceTimer <= 0.0f) {
        const auto& node = npcs[dialogueNPC].dialogueTree.at(currentTreeNodeId);

        // Puoi scorrere solo se l'NPC ha finito di parlare e se ci sono opzioni
        bool isFinished = (dialogueRevealCount >= wrapText(node.npcText, maxChars).length());
        if (isFinished && !node.choices.empty()) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
                selectedChoiceIndex = (selectedChoiceIndex > 0) ? selectedChoiceIndex - 1 : node.choices.size() - 1;
                navDebounceTimer = 0.2f; // Limite velocità cursore
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                selectedChoiceIndex = (selectedChoiceIndex + 1) % node.choices.size();
                navDebounceTimer = 0.2f;
            }
        }
    }

    // 4. Update Grafico e Renderizzatore Scelte
    if(inDialogue && dialogueNPC >= 0) {
        std::string npcText = "";
        std::string choicesText = "";

        if (npcs[dialogueNPC].type == InteractionType::BRANCHING) {
            const auto& node = npcs[dialogueNPC].dialogueTree.at(currentTreeNodeId);
            npcText = wrapText(node.npcText, maxChars);

            // Fai apparire le opzioni solo quando l'NPC ha completato la battuta
            if (dialogueRevealCount >= npcText.length() && !node.choices.empty()) {
                choicesText = "\n\n";
                for (int i = 0; i < node.choices.size(); ++i) {
                    if (i == selectedChoiceIndex) choicesText += "> " + node.choices[i].text + " <\n";
                    else choicesText += "  " + node.choices[i].text + "\n";
                }
            }
        } else {
            npcText = wrapText(npcs[dialogueNPC].dialogues[dialogueIndex], maxChars);
        }

        if (dialogueRevealCount < npcText.length()) {
            dialogueRevealCount += dialogueRevealSpeed * deltaT;
            if(dialogueRevealCount > npcText.length()) dialogueRevealCount = npcText.length();
        }

        int charsToShow = static_cast<int>(dialogueRevealCount);
        std::string visibleText = npcText.substr(0, charsToShow) + choicesText;
        if(visibleText.empty()) visibleText = " ";

        dialogueTextId = txt.print(0.0f, 0.6f, visibleText, dialogueTextId,
                                   "SS", false, false, false,
                                   TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
                                   glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.8f), 1.0f, 1.0f);
    }
}

void DialogueManager::cleanup(TextMaker& txt) {
    if(dialogueTextId != -1) txt.removeText(dialogueTextId);
    if(interactionPromptTextId != -1) txt.removeText(interactionPromptTextId);
}