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
      debounce(false), curDebounce(0), currentTreeNodeId(0), selectedChoiceIndex(0), globalStoryProgress(0) {}

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

    // Lettura Gamepad
    GLFWgamepadstate gamepadState;
    bool hasGamepad = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

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

    // 2. Gestione UI Prompt "Premi E / (A)"
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

    // 3. Input Dialogo e Navigazione (Tastiera + Gamepad)
    bool ePressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) ||
                    (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

    bool enterPressed = (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) ||
                        (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

    if((showInteractionPrompt || inDialogue) && (ePressed || enterPressed) && !debounce) {
        debounce = true;
        curDebounce = 12;

        if(!inDialogue) {
            if (ePressed) {
                // LOGICA RISOLUZIONE STORIA: Scegliamo quale dialogo caricare
                InteractionType tempType = npcs[activeNPC].type;
                const std::vector<std::string>* tempDialogues = &npcs[activeNPC].dialogues;
                const std::map<int, DialogueNode>* tempTree = &npcs[activeNPC].dialogueTree;

                int highestProgress = -1;
                for (const auto& st : npcs[activeNPC].storyStates) {
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

                if(canStart) {
                    inDialogue = true;
                    dialogueNPC = activeNPC;

                    activeType = tempType;
                    activeDialogues = tempDialogues;
                    activeTree = tempTree;

                    dialogueIndex = 0;
                    currentTreeNodeId = 0;
                    selectedChoiceIndex = 0;
                    dialogueRevealCount = 0.0f;

                    glm::vec3 dir = npcs[dialogueNPC].position - player.position;
                    float len = glm::length(glm::vec2(dir.x, dir.z));
                    if(len > 0.001f) {
                        const float RAD2DEG = 180.0f / 3.14159265358979323846f;
                        player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
                    }
                    player.mouseLookInitialized = false;

                    if (activeType == InteractionType::ONE_LINER) {
                        dialogueIndex = rand() % std::max((int)activeDialogues->size(), 1);
                    }

                    dialogueTextId = txt.print(0.0f, 0.6f, "", dialogueTextId, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
                }
            } else {
                debounce = false;
            }
        } else {
            // Logica Avanzamento Dialogo
            std::string currentFull = "";
            if (activeType == InteractionType::BRANCHING) currentFull = activeTree->at(currentTreeNodeId).npcText;
            else currentFull = activeDialogues->at(dialogueIndex);

            bool isRevealFinished = (dialogueRevealCount >= wrapText(currentFull, maxChars).length());

            if(!isRevealFinished) {
                dialogueRevealCount = wrapText(currentFull, maxChars).length();
            } else {
                bool dialogueEnded = false;

                if (activeType == InteractionType::ONE_LINER) {
                    if (ePressed || enterPressed) dialogueEnded = true;
                    else debounce = false;
                }
                else if (activeType == InteractionType::LINEAR) {
                    if (ePressed || enterPressed) {
                        ++dialogueIndex;
                        if(dialogueIndex < static_cast<int>(activeDialogues->size())) {
                            dialogueRevealCount = 0.0f;
                        } else {
                            dialogueEnded = true;
                        }
                    } else {
                        debounce = false;
                    }
                }
                else if (activeType == InteractionType::BRANCHING) {
                    const auto& node = activeTree->at(currentTreeNodeId);
                    if (node.choices.empty()) {
                        if (ePressed || enterPressed) dialogueEnded = true;
                        else debounce = false;
                    } else {
                        if (enterPressed || ePressed) {
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

                if (dialogueEnded) {
                    if(dialogueTextId != -1) dialogueTextId = txt.print(0.0f, 0.8f, "", dialogueTextId);
                    inDialogue = false;
                    dialogueNPC = -1;
                    player.mouseLookInitialized = false;
                }
            }
        }
    }

    if(!ePressed && !enterPressed) debounce = false;
    if(curDebounce > 0) --curDebounce;

    // Navigazione nelle scelte del dialogo a rami (W/S, FRECCE o DPAD/STICK GAMEPAD)
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
        const auto& node = activeTree->at(currentTreeNodeId);
        bool isFinished = (dialogueRevealCount >= wrapText(node.npcText, maxChars).length());

        if (isFinished && !node.choices.empty()) {
            if (upPressed && !upPressedLastFrame) {
                selectedChoiceIndex = (selectedChoiceIndex > 0) ? selectedChoiceIndex - 1 : static_cast<int>(node.choices.size()) - 1;
            }
            if (downPressed && !downPressedLastFrame) {
                selectedChoiceIndex = (selectedChoiceIndex + 1) % static_cast<int>(node.choices.size());
            }
        }
    }
    upPressedLastFrame = upPressed;
    downPressedLastFrame = downPressed;

    // 4. Update Grafico
    if(inDialogue && dialogueNPC >= 0) {
        std::string npcText = "";
        std::string choicesText = "";

        if (activeType == InteractionType::BRANCHING) {
            const auto& node = activeTree->at(currentTreeNodeId);
            npcText = wrapText(node.npcText, maxChars);

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

int DialogueManager::getDialogueNPC() const {
    return inDialogue ? dialogueNPC : -1;
}

void DialogueManager::forceStartDialogue(const std::string& npcName, const std::vector<TavernNPC>& npcs, TextMaker& txt, Player& player) {
    if (inDialogue) return;

    int targetNpcIndex = -1;
    for (size_t i = 0; i < npcs.size(); ++i) {
        if (npcs[i].name == npcName) {
            targetNpcIndex = static_cast<int>(i);
            break;
        }
    }

    if (targetNpcIndex == -1) return;

    InteractionType tempType = npcs[targetNpcIndex].type;
    const std::vector<std::string>* tempDialogues = &npcs[targetNpcIndex].dialogues;
    const std::map<int, DialogueNode>* tempTree = &npcs[targetNpcIndex].dialogueTree;

    int highestProgress = -1;
    for (const auto& st : npcs[targetNpcIndex].storyStates) {
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
        if(len > 0.001f) {
            const float RAD2DEG = 180.0f / 3.14159265358979323846f;
            player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
        }
        player.mouseLookInitialized = false;

        if (activeType == InteractionType::ONE_LINER) {
            dialogueIndex = rand() % std::max((int)activeDialogues->size(), 1);
        }

        dialogueTextId = txt.print(0.0f, 0.6f, "", dialogueTextId, "SS", false, false, false, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
    }
}