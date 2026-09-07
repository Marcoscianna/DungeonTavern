#include "NPC.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <algorithm>

void TavernNPC::update(float deltaT, bool isTalking, const glm::vec3 &playerPos, AnimatedNPCRig &animManager,
                       Scene &SC) {
    bool updateMatrix = false;

    if (isTalking) {
        // L'NPC è in modalità dialogo: blocchiamo il movimento e azzeriamo i timer di attesa
        hasInteracted = true;
        isWaiting = false;

        // Play animazione: se disponibile usiamo la clip di parlato, altrimenti fallback in idle
        int animToPlay = (numAnimations > 1) ? 1 : 0;
        if (numAnimations > 0 && currentAnim != animToPlay) {
            animManager.play(name, animToPlay, 0.2f);
            currentAnim = animToPlay;
        }

        // Calcolo dell'angolo (Yaw) per forzare l'NPC a voltarsi e guardare in faccia il player
        glm::vec3 dir = playerPos - position;
        dir.y = 0.0f;
        if (glm::length(dir) > 0.001f) {
            currentYaw = atan2(dir.x, dir.z);
        }
        updateMatrix = true;
    } else {
        // L'NPC non sta parlando: gestiamo la logica di pattugliamento se ha dei waypoint assegnati
        if (!waypoints.empty()) {
            hasInteracted = true;

            if (isWaiting) {
                // L'NPC è arrivato a un nodo e deve aspettare prima di ripartire
                currentWaitTimer -= deltaT;

                // Play animazione attesa: indice 2, altrimenti idle standard
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
                // L'NPC è in transito verso il prossimo waypoint
                if (numAnimations > 0 && currentAnim != 0) {
                    animManager.play(name, 0, 0.2f); // Indice 0 = Walk
                    currentAnim = 0;
                }

                glm::vec3 target = waypoints[currentWaypoint];
                glm::vec3 dir = target - position;
                float dist = glm::length(dir);

                if (dist < 0.2f) {
                    // Obiettivo raggiunto: verifichiamo se il JSON prevede un tempo di attesa su questo nodo
                    float wTime = (waitTimes.size() > currentWaypoint) ? waitTimes[currentWaypoint] : 0.0f;
                    if (wTime > 0.0f) {
                        isWaiting = true;
                        currentWaitTimer = wTime;
                    } else {
                        // Nessuna sosta, puntiamo direttamente al prossimo waypoint
                        currentWaypoint = (currentWaypoint + 1) % waypoints.size();
                    }
                } else {
                    // Aggiornamento posizione spaziale tramite direzione normalizzata e deltaTime
                    glm::vec3 moveDir = glm::normalize(dir);
                    position += moveDir * speed * deltaT;
                    currentYaw = atan2(moveDir.x, moveDir.z);
                }
                updateMatrix = true;
            }
        } else if (hasInteracted) {
            // Caso per NPC statici (senza waypoint) che hanno appena finito di parlare: ritornano in idle
            if (numAnimations > 0 && currentAnim != 0) {
                animManager.play(name, 0, 0.2f);
                currentAnim = 0;
            }
            updateMatrix = true;
        }
    }

    // Ricostruzione della Model Matrix (Wm) per il rendering
    // Traslazione -> Rotazione Yaw -> Fix -90° asse X -> Scala
    if (updateMatrix) {
        auto it = SC.InstanceIds.find(name);
        if (it != SC.InstanceIds.end()) {
            Instance *inst = SC.I[it->second];
            inst->Wm = glm::translate(glm::mat4(1.0f), position) *
                       glm::rotate(glm::mat4(1.0f), currentYaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                       glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                       glm::scale(glm::mat4(1.0f), scale);
        }
    }
}

std::vector<TavernNPC>
TavernNPC::loadNPCsFromJson(const std::string &filepath, Scene &SC, AnimatedNPCRig &animManager) {
    std::vector<TavernNPC> resultNPCs;
    std::unordered_map<std::string, TavernNPC> npcDataFromJson;

    try {
        std::ifstream ifs(filepath);
        if (ifs.is_open()) {
            nlohmann::json js;
            ifs >> js;
            ifs.close();

            // Scansione del JSON per cercare le istanze marcate come animate
            if (js.contains("instances")) {
                for (const auto &tech: js["instances"]) {
                    if (tech["technique"].template get<std::string>() == "AnimTech") {
                        for (const auto &el: tech["elements"]) {
                            std::string npcId = el["id"].template get<std::string>();

                            if (npcId == "player") continue; // Ignoriamo il player, gestito a parte

                            TavernNPC data;
                            data.name = npcId;

                            // Lettura attributi di base con fallback su valori di default
                            data.prompt = el.value("prompt", "Premi E / (A) per parlare");
                            data.interactionRadius = el.value("interactionRadius", 10.0f);
                            data.speed = el.value("speed", 1.0f);

                            // Lettura array dei waypoint per il pattugliamento
                            if (el.contains("waypoints")) {
                                for (const auto &wp: el["waypoints"]) {
                                    data.waypoints.push_back(glm::vec3(
                                        wp[0].template get<float>(),
                                        wp[1].template get<float>(),
                                        wp[2].template get<float>()
                                    ));
                                }
                            }

                            // Lettura tempi di sosta per ogni waypoint
                            if (el.contains("waitTimes")) {
                                for (const auto &wt: el["waitTimes"]) {
                                    data.waitTimes.push_back(wt.template get<float>());
                                }
                            }

                            // Lambda helper per decodificare in modo ricorsivo l'albero dei dialoghi
                            auto parseTree = [](const nlohmann::json &jsonArray) {
                                std::map<int, DialogueNode> tree;
                                for (const auto &n: jsonArray) {
                                    DialogueNode node;
                                    int nId = n["id"].template get<int>();
                                    node.npcText = n["text"].template get<std::string>();
                                    if (n.contains("choices")) {
                                        for (const auto &c: n["choices"]) {
                                            DialogueChoice choice;
                                            choice.text = c["text"].template get<std::string>();
                                            choice.nextNodeId = c["next"].template get<int>();
                                            choice.setStoryProgress = c.value("setStory", -1);
                                            // Eventuale trigger per avanzamento storia
                                            node.choices.push_back(choice);
                                        }
                                    }
                                    tree[nId] = node;
                                }
                                return tree;
                            };

                            // Configurazione della tipologia di interazione
                            std::string typeStr = el.value("dialogueType", "LINEAR");
                            if (typeStr == "ONE_LINER") data.type = InteractionType::ONE_LINER;
                            else if (typeStr == "BRANCHING") data.type = InteractionType::BRANCHING;
                            else data.type = InteractionType::LINEAR;

                            if (el.contains("dialogues")) {
                                for (const auto &d: el["dialogues"]) {
                                    data.dialogues.push_back(d.template get<std::string>());
                                }
                            }

                            if (el.contains("dialogueTree")) {
                                data.dialogueTree = parseTree(el["dialogueTree"]);
                                data.type = InteractionType::BRANCHING;
                            }

                            // Caricamento stati storia: permette all'NPC di cambiare frasi in base ai progressi del gioco
                            if (el.contains("storyStates")) {
                                for (const auto &st: el["storyStates"]) {
                                    DialogueStateData stateData;
                                    stateData.requiredProgress = st["requiredProgress"].template get<int>();

                                    std::string stType = st.value("dialogueType", "LINEAR");
                                    if (stType == "ONE_LINER") stateData.type = InteractionType::ONE_LINER;
                                    else if (stType == "BRANCHING") stateData.type = InteractionType::BRANCHING;
                                    else stateData.type = InteractionType::LINEAR;

                                    if (st.contains("dialogues")) {
                                        for (const auto &d: st["dialogues"]) {
                                            stateData.dialogues.push_back(d.template get<std::string>());
                                        }
                                    }
                                    if (st.contains("dialogueTree")) {
                                        stateData.dialogueTree = parseTree(st["dialogueTree"]);
                                        stateData.type = InteractionType::BRANCHING;
                                    }
                                    data.storyStates.push_back(stateData);
                                }
                            }

                            npcDataFromJson[npcId] = data;
                        }
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        std::cout << "ERROR loading NPCs from JSON: " << e.what() << "\n";
    }

    // Merge: Inseriamo i dati logici del JSON nei modelli spaziali istanziati dalla Scene
    if (SC.TI != nullptr && SC.TechniqueInstanceCount > 1 && SC.TI[1].I != nullptr) {
        for (int i = 0; i < SC.TI[1].InstanceCount; ++i) {
            const auto &inst = SC.TI[1].I[i];
            TavernNPC npc;
            npc.name = *(inst.id);
            if (npc.name == "player") continue;

            // Estrazione posizione iniziale dalla World Matrix
            npc.position = glm::vec3(inst.Wm[3][0], inst.Wm[3][1], inst.Wm[3][2]);

            // Match con l'hash map precedentemente popolata
            auto it = npcDataFromJson.find(npc.name);
            if (it != npcDataFromJson.end()) {
                npc.prompt = it->second.prompt;
                npc.interactionRadius = it->second.interactionRadius;
                npc.type = it->second.type;
                npc.dialogues = it->second.dialogues;
                npc.dialogueTree = it->second.dialogueTree;
                npc.storyStates = it->second.storyStates;
                npc.speed = it->second.speed;
                npc.waypoints = it->second.waypoints;
                npc.waitTimes = it->second.waitTimes;
            } else {
                // Fallback default se l'NPC è nella scena grafica ma non descritto nella logica JSON
                npc.prompt = "Premi E / (A) per parlare";
                npc.interactionRadius = 10.0f;
                npc.type = InteractionType::LINEAR;
                npc.dialogues.push_back("Benvenuto nella taverna.");
            }
            resultNPCs.push_back(npc);
        }
    }

    // Aggiornamento scala grafica e verifica numero animazioni disponibili
    for (auto &npc: resultNPCs) {
        auto itInst = SC.InstanceIds.find(npc.name);
        if (itInst != SC.InstanceIds.end()) {
            Instance *inst = SC.I[itInst->second];
            npc.scale = glm::vec3(glm::length(glm::vec3(inst->Wm[0])),
                                  glm::length(glm::vec3(inst->Wm[1])),
                                  glm::length(glm::vec3(inst->Wm[2])));
        }

        AnimNPC *animNpc = animManager.find(npc.name);
        if (animNpc) {
            npc.numAnimations = animNpc->blender.segments.size();
        }
    }

    // Hardcode fallback di sicurezza in caso di rottura totale del JSON
    if (resultNPCs.empty()) {
        resultNPCs = {
            {
                "door_guard_r", glm::vec3(9.0f, 0.0f, -8.0f), 10.0f, "Premi E / (A) per parlare",
                InteractionType::LINEAR, {"Benvenuto nella taverna."}, {}
            },
            {
                "door_guard_l", glm::vec3(13.8f, 0.0f, -8.0f), 10.0f, "Premi E / (A) per parlare",
                InteractionType::LINEAR, {"La porta è chiusa per stanotte."}, {}
            }
        };
    }

    return resultNPCs;
}