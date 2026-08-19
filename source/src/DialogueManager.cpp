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
      debounce(false), curDebounce(0) {}

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

    // 3. Input Dialogo
    if(showInteractionPrompt && glfwGetKey(window, GLFW_KEY_E) && !debounce) {
        debounce = true;
        curDebounce = 12;

        if(!inDialogue) {
            if(activeNPC >= 0 && !npcs[activeNPC].dialogues.empty()) {
                inDialogue = true;
                dialogueNPC = activeNPC;
                dialogueIndex = 0;
                dialogueRevealCount = 0.0f;

                // Fix telecamera
                glm::vec3 dir = npcs[dialogueNPC].position - player.position;
                float len = glm::length(glm::vec2(dir.x, dir.z));
                if(len > 0.001f) {
                    const float RAD2DEG = 180.0f / 3.14159265358979323846f;
                    player.yaw = atan2(dir.z, dir.x) * RAD2DEG;
                }
                player.mouseLookInitialized = false;

                dialogueTextId = txt.print(0.0f, 0.8f, "", dialogueTextId,
                                           "SS", false, false, false,
                                           TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
                                           glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
            }
        } else {
            std::string currentFull = wrapText(npcs[dialogueNPC].dialogues[dialogueIndex], maxChars);

            if(dialogueRevealCount < currentFull.length()) {
                dialogueRevealCount = currentFull.length(); // Salta l'animazione
            } else {
                ++dialogueIndex;
                if(dialogueIndex < static_cast<int>(npcs[dialogueNPC].dialogues.size())) {
                    dialogueRevealCount = 0.0f;
                } else {
                    // Fine Dialogo
                    if(dialogueTextId != -1) {
                        dialogueTextId = txt.print(0.0f, 0.8f, "", dialogueTextId,
                                                   "SS", false, false, false,
                                                   TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
                                                   glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
                    }
                    inDialogue = false;
                    dialogueNPC = -1;
                    dialogueIndex = 0;
                    player.mouseLookInitialized = false;
                }
            }
        }
    }

    if(!glfwGetKey(window, GLFW_KEY_E)) debounce = false;
    if(curDebounce > 0) --curDebounce;

    // 4. Update Grafico Effetto Macchina da Scrivere
    if(inDialogue && dialogueNPC >= 0) {
        std::string currentFull = wrapText(npcs[dialogueNPC].dialogues[dialogueIndex], maxChars);

        if (dialogueRevealCount < currentFull.length()) {
            dialogueRevealCount += dialogueRevealSpeed * deltaT;
            if(dialogueRevealCount > currentFull.length()) dialogueRevealCount = currentFull.length();
        }

        int charsToShow = static_cast<int>(dialogueRevealCount);
        std::string visibleText = currentFull.substr(0, charsToShow);
        if(visibleText.empty()) visibleText = " ";

        dialogueTextId = txt.print(0.0f, 0.8f, visibleText, dialogueTextId,
                                   "SS", false, false, false,
                                   TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
                                   glm::vec4(1.0f,1.0f,1.0f,1.0f), glm::vec4(0.0f), glm::vec4(0.0f,0.0f,0.0f,0.6f), 1.0f, 1.0f);
    }
}

void DialogueManager::cleanup(TextMaker& txt) {
    if(dialogueTextId != -1) txt.removeText(dialogueTextId);
    if(interactionPromptTextId != -1) txt.removeText(interactionPromptTextId);
}