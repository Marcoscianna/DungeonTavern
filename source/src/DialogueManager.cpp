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
      debounce(false), curDebounce(0), currentTreeNodeId(0), selectedChoiceIndex(0), globalStoryProgress(0) {
}

bool DialogueManager::isDialogueActive() const {
    return inDialogue;
}

// Utility per spezzare le righe di testo in base alla larghezza dello schermo,
// evita che i testi dei dialoghi escano dai bordi della finestra.
std::string DialogueManager::wrapText(const std::string &text, int maxLineLen) {
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

void DialogueManager::update(GLFWwindow *window, float deltaT, Player &player,
                             const std::vector<TavernNPC> &npcs, TextMaker &txt, int windowWidth) {
    // Calcolo dinamico dei caratteri massimi in base alla risoluzione
    int maxChars = std::max(15, (int) (windowWidth / 22));

    // Lettura dello stato del controller
    GLFWgamepadstate gamepadState;
    bool hasGamepad = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

    // 1. Calcolo Distanze
    // Scansioniamo la lista degli NPC per trovare quello più vicino al player
    // all'interno del raggio di interazione
    showInteractionPrompt = false;
    activeNPC = -1;
    float bestDistance = 99999.0f;

    for (size_t i = 0; i < npcs.size(); ++i) {
        glm::vec2 playerPosXZ(player.position.x, player.position.z);
        glm::vec2 npcPosXZ(npcs[i].position.x, npcs[i].position.z);
        float d = glm::length(playerPosXZ - npcPosXZ);
        float hDiff = std::abs(player.position.y - npcs[i].position.y);

        if (d < npcs[i].interactionRadius && hDiff <= 4.0f && d < bestDistance) {
            bestDistance = d;
            activeNPC = static_cast<int>(i);
            showInteractionPrompt = true;
        }
    }

    // 2. Gestione UI del Prompt di interazione
    // Mostriamo a schermo il suggerimento (es. "Premi E per parlare") solo se c'è un NPC a tiro
    if (showInteractionPrompt && !inDialogue) {
        if (interactionPromptTextId == -1 || activeNPC != lastActiveNPC) {
            std::string p = wrapText(npcs[activeNPC].prompt, maxChars);
            interactionPromptTextId = txt.print(0.0f, -0.8f, p, interactionPromptTextId,
                                                "SS", false, false, false,
                                                TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.5f),
                                                1.0f, 1.0f);
            lastActiveNPC = activeNPC;
        }
    } else {
        // Nascondiamo il prompt se ci allontaniamo o se siamo già in un dialogo
        if (interactionPromptTextId != -1) {
            interactionPromptTextId = txt.print(0.0f, -0.8f, "", interactionPromptTextId,
                                                "SS", false, false, false,
                                                TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.5f),
                                                1.0f, 1.0f);
            lastActiveNPC = -1;
        }
    }

    // 3. Logica di Input (Tastiera + Gamepad) per avviare o mandare avanti i dialoghi
    bool ePressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

    bool enterPressed = (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) ||
                        (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

    // Gestione debounce per evitare che la pressione di un tasto salti più righe di dialogo in un colpo solo
    if ((showInteractionPrompt || inDialogue) && (ePressed || enterPressed) && !debounce) {
        debounce = true;
        curDebounce = 12;

        if (!inDialogue) {
            if (ePressed) {
                // Inizio dialogo: filtriamo le battute disponibili in base allo stato attuale
                // della storia (globalStoryProgress) per caricare il ramo o la frase corretta
                InteractionType tempType = npcs[activeNPC].type;
                const std::vector<std::string> *tempDialogues = &npcs[activeNPC].dialogues;
                const std::map<int, DialogueNode> *tempTree = &npcs[activeNPC].dialogueTree;

                int highestProgress = -1;
                for (const auto &st: npcs[activeNPC].storyStates) {
                    if (st.requiredProgress <= globalStoryProgress && st.requiredProgress > highestProgress) {
                        highestProgress = st.requiredProgress;
                        tempType = st.type;
                        tempDialogues = &st.dialogues;
                        tempTree = &st.dialogueTree;
                    }
                }

                bool canStart = false;
                if (activeNPC >= 0) {
                    if (tempType == InteractionType::BRANCHING && !tempTree->empty()) canStart = true;
                    if (tempType != InteractionType::BRANCHING && !tempDialogues->empty()) canStart = true;
                }

                if (canStart) {
                    inDialogue = true;
                    dialogueNPC = activeNPC;

                    activeType = tempType;
                    activeDialogues = tempDialogues;
                    activeTree = tempTree;

                    dialogueIndex = 0;
                    currentTreeNodeId = 0;
                    selectedChoiceIndex = 0;
                    dialogueRevealCount = 0.0f; // Azzera il contatore per l'effetto macchina da scrivere

                    // Calcolo rotazione: forza il player a guardare in faccia l'NPC
                    glm::vec3 dir = npcs[dialogueNPC].position - player.position;
                    float len = glm::length(glm::vec2(dir.x, dir.z));
                    if (len > 0.001f) {
                        const float RAD2DEG = 180.0f / 3.14159265358979323846f;
                        player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
                    }
                    player.mouseLookInitialized = false;

                    // Per le frasi randomiche "al volo" ne peschiamo una a caso
                    if (activeType == InteractionType::ONE_LINER) {
                        dialogueIndex = rand() % std::max((int) activeDialogues->size(), 1);
                    }

                    // Inizializza l'elemento UI per il testo
                    dialogueTextId = txt.print(0.0f, 0.6f, "", dialogueTextId, "SS", false, false, false, TAL_CENTER,
                                               TRH_CENTER, TRV_BOTTOM, glm::vec4(1.0f), glm::vec4(0.0f),
                                               glm::vec4(0.0f, 0.0f, 0.0f, 0.6f), 1.0f, 1.0f);
                }
            } else {
                debounce = false;
            }
        } else {
            // Avanzamento del dialogo attivo
            std::string currentFull = "";
            if (activeType == InteractionType::BRANCHING) currentFull = activeTree->at(currentTreeNodeId).npcText;
            else currentFull = activeDialogues->at(dialogueIndex);

            // Se il testo non è ancora apparso tutto, forziamo il completamento immediato (skip)
            bool isRevealFinished = (dialogueRevealCount >= wrapText(currentFull, maxChars).length());

            if (!isRevealFinished) {
                dialogueRevealCount = wrapText(currentFull, maxChars).length();
            } else {
                bool dialogueEnded = false;

                if (activeType == InteractionType::ONE_LINER) {
                    // Le One-liner si chiudono alla prima pressione
                    if (ePressed || enterPressed) dialogueEnded = true;
                    else debounce = false;
                } else if (activeType == InteractionType::LINEAR) {
                    // Dialogo lineare: passa alla frase successiva
                    if (ePressed || enterPressed) {
                        ++dialogueIndex;
                        if (dialogueIndex < static_cast<int>(activeDialogues->size())) {
                            dialogueRevealCount = 0.0f;
                        } else {
                            dialogueEnded = true; // Fine dell'array, chiudi il dialogo
                        }
                    } else {
                        debounce = false;
                    }
                } else if (activeType == InteractionType::BRANCHING) {
                    // Dialogo a bivi: naviga l'albero in base alla scelta selezionata
                    const auto &node = activeTree->at(currentTreeNodeId);
                    if (node.choices.empty()) {
                        if (ePressed || enterPressed) dialogueEnded = true;
                        else debounce = false;
                    } else {
                        if (enterPressed || ePressed) {
                            // Applica gli eventuali effetti della scelta sulla progressione della storia
                            int setStory = node.choices[selectedChoiceIndex].setStoryProgress;
                            if (setStory != -1) {
                                globalStoryProgress = setStory;
                            }

                            int nextId = node.choices[selectedChoiceIndex].nextNodeId;
                            if (nextId == -1) {
                                dialogueEnded = true;
                            } else {
                                currentTreeNodeId = nextId;
                                selectedChoiceIndex = 0;
                                dialogueRevealCount = 0.0f;
                            }
                        } else {
                            debounce = false;
                        }
                    }
                }

                // Pulizia a fine conversazione
                if (dialogueEnded) {
                    if (dialogueTextId != -1) dialogueTextId = txt.print(0.0f, 0.8f, "", dialogueTextId);
                    inDialogue = false;
                    dialogueNPC = -1;
                    player.mouseLookInitialized = false;
                }
            }
        }
    }

    if (!ePressed && !enterPressed) debounce = false;
    if (curDebounce > 0) --curDebounce;

    // Gestione input per scorrere tra le opzioni di risposta nei dialoghi a bivi
    static bool upPressedLastFrame = false;
    static bool downPressedLastFrame = false;

    bool upPressed = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
                     (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) ||
                     (hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                                     gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f));

    bool downPressed = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) ||
                       (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) ||
                       (hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                                       gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f));

    if (inDialogue && dialogueNPC >= 0 && activeType == InteractionType::BRANCHING) {
        const auto &node = activeTree->at(currentTreeNodeId);
        bool isFinished = (dialogueRevealCount >= wrapText(node.npcText, maxChars).length());

        // Consenti di muovere il cursore solo quando l'NPC ha finito di "parlare"
        if (isFinished && !node.choices.empty()) {
            if (upPressed && !upPressedLastFrame) {
                selectedChoiceIndex = (selectedChoiceIndex > 0)
                                          ? selectedChoiceIndex - 1
                                          : static_cast<int>(node.choices.size()) - 1;
            }
            if (downPressed && !downPressedLastFrame) {
                selectedChoiceIndex = (selectedChoiceIndex + 1) % static_cast<int>(node.choices.size());
            }
        }
    }
    upPressedLastFrame = upPressed;
    downPressedLastFrame = downPressed;

    // 4. Update Grafico (Effetto macchina da scrivere e rendering)
    if (inDialogue && dialogueNPC >= 0) {
        std::string npcText = "";
        std::string choicesText = "";

        if (activeType == InteractionType::BRANCHING) {
            const auto &node = activeTree->at(currentTreeNodeId);
            npcText = wrapText(node.npcText, maxChars);

            // Mostra le opzioni di risposta solo al termine dell'animazione del testo principale
            if (dialogueRevealCount >= npcText.length() && !node.choices.empty()) {
                choicesText = "\n\n";
                for (size_t i = 0; i < node.choices.size(); ++i) {
                    if (static_cast<int>(i) == selectedChoiceIndex) choicesText += "> " + node.choices[i].text + " <\n";
                    else choicesText += "  " + node.choices[i].text + "\n";
                }
            }
        } else {
            npcText = wrapText(activeDialogues->at(dialogueIndex), maxChars);
        }

        // Avanzamento contatore per svelare i caratteri progressivamente
        if (dialogueRevealCount < npcText.length()) {
            dialogueRevealCount += dialogueRevealSpeed * deltaT;
            if (dialogueRevealCount > npcText.length()) dialogueRevealCount = npcText.length();
        }

        // Taglia la stringa per simulare il testo che si compone dinamicamente, poi accoda le scelte
        int charsToShow = static_cast<int>(dialogueRevealCount);
        std::string visibleText = npcText.substr(0, charsToShow) + choicesText;
        if (visibleText.empty()) visibleText = " ";

        // Aggiorna l'oggetto text maker a schermo
        dialogueTextId = txt.print(0.0f, 0.6f, visibleText, dialogueTextId,
                                   "SS", false, false, false,
                                   TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
                                   glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.8f), 1.0f, 1.0f);
    }
}

void DialogueManager::cleanup(TextMaker &txt) {
    if (dialogueTextId != -1) txt.removeText(dialogueTextId);
    if (interactionPromptTextId != -1) txt.removeText(interactionPromptTextId);
}

int DialogueManager::getDialogueNPC() const {
    return inDialogue ? dialogueNPC : -1;
}

// Forza l'inizio di un dialogo ignorando i check della distanza.
// Comodo per le cutscene o quando il player calpesta un trigger invisibile (es. la porta della taverna)
void DialogueManager::forceStartDialogue(const std::string &npcName, const std::vector<TavernNPC> &npcs, TextMaker &txt,
                                         Player &player) {
    if (inDialogue) return;

    // Cerca l'NPC desiderato per nome all'interno della lista
    int targetNpcIndex = -1;
    for (size_t i = 0; i < npcs.size(); ++i) {
        if (npcs[i].name == npcName) {
            targetNpcIndex = static_cast<int>(i);
            break;
        }
    }

    if (targetNpcIndex == -1) return;

    InteractionType tempType = npcs[targetNpcIndex].type;
    const std::vector<std::string> *tempDialogues = &npcs[targetNpcIndex].dialogues;
    const std::map<int, DialogueNode> *tempTree = &npcs[targetNpcIndex].dialogueTree;

    int highestProgress = -1;
    for (const auto &st: npcs[targetNpcIndex].storyStates) {
        if (st.requiredProgress <= globalStoryProgress && st.requiredProgress > highestProgress) {
            highestProgress = st.requiredProgress;
            tempType = st.type;
            tempDialogues = &st.dialogues;
            tempTree = &st.dialogueTree;
        }
    }

    bool canStart = false;
    if (tempType == InteractionType::BRANCHING && !tempTree->empty()) canStart = true;
    if (tempType != InteractionType::BRANCHING && !tempDialogues->empty()) canStart = true;

    if (canStart) {
        inDialogue = true;
        dialogueNPC = targetNpcIndex;

        activeType = tempType;
        activeDialogues = tempDialogues;
        activeTree = tempTree;

        dialogueIndex = 0;
        currentTreeNodeId = 0;
        selectedChoiceIndex = 0;
        dialogueRevealCount = 0.0f;

        glm::vec3 dir = npcs[dialogueNPC].position - player.position;
        float len = glm::length(glm::vec2(dir.x, dir.z));
        if (len > 0.001f) {
            const float RAD2DEG = 180.0f / 3.14159265358979323846f;
            player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
        }
        player.mouseLookInitialized = false;

        if (activeType == InteractionType::ONE_LINER) {
            dialogueIndex = rand() % std::max((int) activeDialogues->size(), 1);
        }

        dialogueTextId = txt.print(0.0f, 0.6f, "", dialogueTextId, "SS", false, false, false, TAL_CENTER, TRH_CENTER,
                                   TRV_BOTTOM, glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.6f),
                                   1.0f, 1.0f);
    }
}