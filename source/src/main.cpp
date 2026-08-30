// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include <json.hpp>

#include "NPC.hpp"
#include "Player.hpp"
#include "modules/TextMaker.hpp"
#include "DialogueManager.hpp"
#include "LightManager.hpp"
#include "PhysicsManager.hpp"
#include "MissionManager.hpp"

struct UniformBufferObject {
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 UV;
    glm::vec3 norm;
};

// Il vertice per i modelli animati
struct VertexAnim {
    glm::vec3 pos;
    glm::vec2 UV;
    glm::vec3 norm;
    glm::vec4 jointWeights;
    glm::uvec4 jointIndices;
};

// MAIN !
class DungeonTavern : public BaseProject {
protected:
    // Descriptor Layouts
    DescriptorSetLayout DSLlocal, DSLglobal, DSLemissive;

    // Animazioni
    DescriptorSetLayout DSLanim;
    VertexDescriptor VDanim;
    Pipeline Panim;

    // Vertex formants, Pipelines and Render passes
    VertexDescriptor VD;
    RenderPass RP;
    Pipeline P, Pemissive, Pwood, Pstone, Pmetal, Psky;

    // Ombre
    RenderPass RPshadow;
    Pipeline Pshadow, PanimShadow;
    bool shadowsEnabled = true;
    bool lPressedLastFrame = false;

    // Models, textures and Descriptors (values assigned to the uniforms)
    DescriptorSet DSglobal;

    // To support loading assets from a scene.json file
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef> PRs;

    // to provide textual feedback
    TextMaker txt;

    // Other application parameters
    float Ar; // Aspect ratio

    // Oggetto Player per gestire movimento e visuale
    Player player;

    // Oggetto per gestire la fisica degli oggetti
    PhysicsManager physicsManager;

    // Oggetto per gestire il ciclo giorno/notte e le luci dinamiche
    LightManager lightManager;

    // ==========================================
    // SISTEMA NPC: Logica e Grafica
    // ==========================================

    // 1. Logica: Lista degli NPC per gestire collisioni e dialoghi
    std::vector<TavernNPC> tavernNPCs;

    // 2. Grafica: Gestore (Manager) dei modelli 3D animati
    AnimatedNPCRig npcAnimManager;

    // 3. Manager dei Dialoghi: Gestisce stato, UI e interazioni
    DialogueManager dialogueManager;

    // 4. Manager delle Missioni
    MissionManager missionManager;

    // 5. Collider per il trigger della porta
    Collider triggerPorta;
    bool triggerPortaAttivato = false;
public:
    DungeonTavern() : Ar(4.0f / 3.0f) {
    } // Costruttore molto più pulito ora

    void setWindowParameters() override {
        windowWidth = 800;
        windowHeight = 600;
        windowTitle = "Dungeon Tavern";
        windowResizable = GLFW_TRUE;
        Ar = 4.0f / 3.0f;
    }

    void onWindowResize(int w, int h) override {
        std::cout << "Window resized to: " << w << " x " << h << "\n";
        Ar = (float) w / (float) h;
        RP.width = w;
        RP.height = h;
        txt.resizeScreen((int) w, (int) h);
    }

    void localInit() override {
        // 1. Crea il Layout per l'UBO Animato
        DSLanim.init(this, {
                         {
                             0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,
                             sizeof(AnimUniformBufferObject), 1
                         },
                         {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
                         // Indice 0: Diffuse
                         {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1},
                         // Indice 1: Normal
                         {
                             3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1
                         } // Indice 2: Specular
                     });

        DSLlocal.init(this, {
                          {
                              0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,
                              sizeof(UniformBufferObject), 1
                          },
                          {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
                      });

        DSLglobal.init(this, {
                           {
                               0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
                               sizeof(GlobalUniformBufferObject), 1
                           },
                           {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
                       });

        DSLemissive.init(this, {
                             {
                                 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,
                                 sizeof(UniformBufferObject), 1
                             },
                             {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
                             {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}
                         });

        VD.init(this, {
                    {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
                }, {
                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos), sizeof(glm::vec3), POSITION},
                    {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV), sizeof(glm::vec2), UV},
                    {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, norm), sizeof(glm::vec3), NORMAL}
                });

        VDanim.init(this, {
                        {0, sizeof(VertexAnim), VK_VERTEX_INPUT_RATE_VERTEX}
                    }, {
                        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexAnim, pos), sizeof(glm::vec3), POSITION},
                        {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexAnim, UV), sizeof(glm::vec2), UV},
                        {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexAnim, norm), sizeof(glm::vec3), NORMAL},
                        {
                            0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexAnim, jointWeights), sizeof(glm::vec4),
                            JOINTWEIGHT
                        },
                        {
                            0, 4, VK_FORMAT_R32G32B32A32_UINT, offsetof(VertexAnim, jointIndices), sizeof(glm::uvec4),
                            JOINTINDEX
                        }
                    });

        RP.init(this);
        RP.properties[0].clearValue = {0.01f, 0.02f, 0.02f, 1.0f};

        P.init(this, &VD, "shaders/static.vert.spv",
               "shaders/blinn.frag.spv",
               {&DSLglobal, &DSLlocal});

        Panim.init(this, &VDanim,
                   "shaders/skinning.vert.spv",
                   "shaders/anim_blinn.frag.spv",
                   {&DSLglobal, &DSLanim});

        Pemissive.init(this, &VD, "shaders/static.vert.spv",
                       "shaders/emissive.frag.spv",
                       {&DSLglobal, &DSLemissive});
        Pemissive.setCullMode(VK_CULL_MODE_NONE);

        Psky.init(this, &VD, "shaders/static.vert.spv",
               "shaders/sky.frag.spv",
               {&DSLglobal, &DSLemissive});

        Pwood.init(this, &VD, "shaders/static.vert.spv", "shaders/wood.frag.spv", {&DSLglobal, &DSLlocal});
        Pstone.init(this, &VD, "shaders/static.vert.spv", "shaders/stone.frag.spv", {&DSLglobal, &DSLlocal});
        Pmetal.init(this, &VD, "shaders/static.vert.spv", "shaders/metal.frag.spv", {&DSLglobal, &DSLlocal});

        Pshadow.init(this, &VD, "shaders/shadow.vert.spv", "shaders/shadow.frag.spv", {&DSLglobal, &DSLlocal});
        Pshadow.setCullMode(VK_CULL_MODE_NONE);
        PanimShadow.init(this, &VDanim, "shaders/shadow_anim.vert.spv", "shaders/shadow.frag.spv",
                         {&DSLglobal, &DSLanim});
        PanimShadow.setCullMode(VK_CULL_MODE_NONE);


        DPSZs.uniformBlocksInPool = 300;
        DPSZs.texturesInPool = 150;
        DPSZs.setsInPool = 300;

        VDRs.resize(2);
        VDRs[0].init("VDposUV", &VD);
        VDRs[1].init("VDanim", &VDanim);

        PRs.resize(7);
        PRs[0].init("BlinnPos", {
                        {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                        {.P = &P, .texDefs = {{}, {{true, 0, {}}}}}
                    }, 1, &VD);
        PRs[1].init("AnimTech", {
                        {.P = &PanimShadow, .texDefs = {{}, {{true, 0, {}}, {true, 1, {}}, {true, 2, {}}}}},
                        {.P = &Panim, .texDefs = {{}, {{true, 0, {}}, {true, 1, {}}, {true, 2, {}}}}}
                    }, 3, &VDanim);
        PRs[2].init("EmissiveTech", {
                        {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                        {.P = &Pemissive, .texDefs = {{}, {{true, 0, {}}, {true, 1, {}}}}}
                    }, 2, &VD);
        PRs[3].init("WoodTech", {
                        {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                        {.P = &Pwood, .texDefs = {{}, {{true, 0, {}}}}}
                    }, 1, &VD);
        PRs[4].init("StoneTech", {
                        {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                        {.P = &Pstone, .texDefs = {{}, {{true, 0, {}}}}}
                    }, 1, &VD);
        PRs[5].init("MetalTech", {
                        {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                        {.P = &Pmetal, .texDefs = {{}, {{true, 0, {}}}}}
                    }, 1, &VD);
        PRs[6].init("SkyTech", {
                                {.P = &Pshadow, .texDefs = {{}, {{true, 0, {}}}}},
                                {.P = &Psky, .texDefs = {{}, {{true, 0, {}}, {true, 1, {}}}}}
                            }, 2, &VD);

        if (SC.init(this, 2, VDRs, PRs, "assets/scenes/scene.json") != 0) {
            std::cout << "ERROR LOADING THE SCENE\n";
            exit(0);
        }

        // Inizializzazione moduli di base
        txt.init(this, (int) windowWidth, (int) windowHeight);
        player.init(glm::vec3(11.5f, 3.0f, 16.0f), 90.0f, 0.0f, 0.5f);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // =====================================================================
        // CONFIGURAZIONE MANAGER
        // =====================================================================

        npcAnimManager.init({
            {
                "door_guard_r", "assets/models/npc/guard/guard_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
            {
                "door_guard_l", "assets/models/npc/guard/guard_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
            {
                "peasant_npc", "assets/models/npc/peasant/peasant_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
            {
                "fighter_npc", "assets/models/npc/fighter/fighter_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
                {"archer_npc", "assets/models/npc/archer/archer.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
            {
                "male_npc", "assets/models/npc/male/male_npc.gltf", "mixamo.com.001", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
            {
                "male_npc2", "assets/models/npc/male/male2.gltf", "maleCombined", 0, glm::mat4(1.0f),
                {
                    {0, 29, 1.0f, 0}, // Segmento 0 (Prima animazione, da frame 0 a 31)
                    {31, 120, 1.0f, 0}, // Segmento 1 (Seconda animazione, da frame 31 a 121)
                    {122, 300, 1.0f, 0} // Segmento 2 (Terza animazione, da frame 121 a 300)
                }
            },
            {
                "player", "assets/models/npc/guard/guard_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f),
                {{0, 255, 1.0f, 0}}
            },
        });
        tavernNPCs = TavernNPC::loadNPCsFromJson("assets/scenes/scene.json", SC, npcAnimManager);
        physicsManager.init(SC, "assets/scenes/scene.json", 0.3f);
        lightManager.init(12.0f);
        lightManager.loadLightsFromJson("assets/scenes/scene.json");

        // =====================================================================
        // CONFIGURAZIONE MISSIONI
        // =====================================================================

        // Missione Boccali: Parte a story=1, cerca i "boccale", si consegnano su "tavolo_quadrato1", imposta story=2
        missionManager.addCollectionMission(1, "boccale", "tavolo_quadrato1", 2, "Trova i boccali", 3.0f, 5.0f);

        // Missione Piatti: Parte a story=4, cerca i "piatto", si consegnano su "tavolo_quadrato1", imposta story=5
        missionManager.addCollectionMission(4, "piatto", "tavolo_quadrato1", 5, "Trova i piatti", 3.0f, 5.0f);

        // =====================================================================
        // CONFIGURAZIONE TRIGGER
        // =====================================================================
        triggerPorta.initAABB(12.5643, -0.363361, -6.37082, 9.56568, 4.13063, -6.6541);
        triggerPorta.setWorldMatrix(glm::mat4(1.0f));
        SC.ColShow.show(&triggerPorta);

        // NON TOGLIERE: Trucco anti-crash per il buffer vuoto del TextMaker
        txt.print(-100.0f, -100.0f, " ");

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsInit() override {
        //texture depth 2048x2048
        RPshadow.init(this, 2048, 2048, 1, RenderPass::getStandardAttchmentsProperties(AT_DEPTH_ONLY, this),
                      RenderPass::getStandardDependencies(ATDEP_NO_DEP), true);
        RPshadow.create();
        RP.create();

        Pshadow.create(&RPshadow);
        PanimShadow.create(&RPshadow);
        P.create(&RP);
        Panim.create(&RP);
        Pemissive.create(&RP);
        Pwood.create(&RP);
        Pstone.create(&RP);
        Pmetal.create(&RP);
        Psky.create(&RP);

        DSglobal.init(this, &DSLglobal, {RPshadow.attachments[0].getViewAndSampler()});

        // INIEZIONE DELLA SHADOW MAP NELLE TECNICHE
        TextureDefs shadowTexDef = {false, 0, RPshadow.attachments[0].getViewAndSampler()};
        // Per le 6 tecniche
        for (int t = 0; t < 7; t++) {
            // Per i 2 Passaggi (Shadow, Color)
            for (int p = 0; p < 2; p++) {
                // Inseriamo la shadow map nel Set 0
                PRs[t].PT[p].texDefs[0].push_back(shadowTexDef);
            }
        }

        SC.pipelinesAndDescriptorSetsInit();
        txt.pipelinesAndDescriptorSetsInit();
    }

    void pipelinesAndDescriptorSetsCleanup() override {
        Pshadow.cleanup();
        PanimShadow.cleanup();
        P.cleanup();
        Panim.cleanup();
        Pemissive.cleanup();
        Pwood.cleanup();
        Pstone.cleanup();
        Pmetal.cleanup();
        Psky.cleanup();
        RP.cleanup();
        RPshadow.cleanup();

        DSglobal.cleanup();

        SC.pipelinesAndDescriptorSetsCleanup();
        txt.pipelinesAndDescriptorSetsCleanup();
    }

    void localCleanup() override {
        DSLlocal.cleanup();
        DSLglobal.cleanup();
        DSLanim.cleanup();
        DSLemissive.cleanup();

        Pshadow.destroy();
        PanimShadow.destroy();
        P.destroy();
        Panim.destroy();
        Pemissive.destroy();
        Pwood.destroy();
        Pstone.destroy();
        Pmetal.destroy();
        Psky.destroy();
        npcAnimManager.cleanup();
        dialogueManager.cleanup(txt);
        missionManager.cleanup(txt);

        RP.destroy();
        RPshadow.destroy();

        SC.localCleanup();
        txt.localCleanup();
    }

    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
        auto *T = static_cast<DungeonTavern *>(Params);
        T->populateCommandBuffer(commandBuffer, currentImage);
    }

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
        // PASS 0: Disegna le ombre nella mappa del sole
        RPshadow.begin(commandBuffer, 0);
        SC.populateCommandBuffer(commandBuffer, 0, currentImage);
        RPshadow.end(commandBuffer);

        // PASS 1: Disegna la scena su schermo
        RP.begin(commandBuffer, currentImage);
        SC.populateCommandBuffer(commandBuffer, 1, currentImage);
        RP.end(commandBuffer);
    }

    void updateUniformBuffer(uint32_t currentImage) override {
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float deltaT = static_cast<float>(now - lastTime);
        lastTime = now;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // --- TOGGLE OMBRE ---
        bool lPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        if (lPressed && !lPressedLastFrame) {
            shadowsEnabled = !shadowsEnabled;
        }
        lPressedLastFrame = lPressed;

        // =========================================================
        // 1. UPDATE GAME LOGIC (Dialoghi, Input, Collisioni)
        // =========================================================
        glm::vec3 oldPlayerPos = player.position;

        dialogueManager.update(window, deltaT, player, tavernNPCs, txt, windowWidth);

        // UPDATE MISSIONI
        missionManager.update(dialogueManager, SC, physicsManager, txt);

        int talkingNPC = dialogueManager.getDialogueNPC();

        for (size_t i = 0; i < tavernNPCs.size(); ++i) {
            bool isTalking = (i == talkingNPC);
            tavernNPCs[i].update(deltaT, isTalking, player.position, npcAnimManager, SC);
        }

        lightManager.update(deltaT, window);

        // Il giocatore può muoversi solo se NON sta parlando
        if (!dialogueManager.isDialogueActive()) {
            player.processInput(window, deltaT, SC, physicsManager);
        }

        if (dialogueManager.getStoryProgress() < 1 && !dialogueManager.isDialogueActive()) {

            // Controlla la collisione tra il giocatore e il trigger
            if (player.playerCollider && triggerPorta.collidesWith(*(player.playerCollider))) {

                // Lo facciamo scattare solo se non è già stato attivato
                if (!triggerPortaAttivato) {
                    dialogueManager.forceStartDialogue("door_guard_l", tavernNPCs, txt, player);
                    triggerPortaAttivato = true;
                }
                player.position = oldPlayerPos;
            } else {
                triggerPortaAttivato = false;
            }
        }

        // =========================================================
        // 2. UPDATE GRAPHICS (Matrici, Uniforms, Rendering)
        // =========================================================

        // Matrice ViewProjection calcolata dal Player
        glm::mat4 ViewPrj = player.getViewProjectionMatrix(Ar);
        SC.updateColliderVisualizer(currentImage, ViewPrj);

        // =========================================================
        // AGGIORNAMENTO DINAMICO COLORE DEL CIELO (CLEAR VALUE)
        // =========================================================
        glm::vec4 sky = lightManager.getSkyColor();
        RP.properties[0].clearValue = {sky.r, sky.g, sky.b, 1.0f};

        // =========================================================
        // CALCOLO DELLA TELECAMERA DEL SOLE (SHADOW MAPPING)
        // =========================================================
        GlobalUniformBufferObject gubo{};
        gubo.eyePos = player.position;
        gubo.shadowToggle = shadowsEnabled ? 1.0f : 0.0f;
        lightManager.applyToGUBO(gubo);

        glm::mat4 lightProj = glm::ortho(-50.0f, 50.0f, -150.0f, 150.0f, -50.0f, 100.0f);
        lightProj[1][1] *= -1;

        // Scegliamo un punto fisso al centro della taverna
        glm::vec3 tavernCenter = glm::vec3(11.0f, 0.0f, -25.0f);

        // Posizioniamo il sole rispetto al centro della taverna
        glm::vec3 lightPos = tavernCenter - gubo.lightDir * 50.0f;
        glm::mat4 lightViewMat = glm::lookAt(lightPos, tavernCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        gubo.lightVP = lightProj * lightViewMat;

        DSglobal.map((int) currentImage, &gubo, 0);

        UniformBufferObject ubo{};

        // --- AGGIORNA TUTTI I MATERIALI STATICI (Tecniche 0, 2, 3, 4, 5, 6) ---
        int staticTechniques[] = {0, 2, 3, 4, 5, 6};

        // ID dello Skydome attivo fornito dal LightManager
        std::string activeSkyId = lightManager.getCurrentSkydomeInstanceId();

        for (int t : staticTechniques) {
            if (t < SC.TechniqueInstanceCount && SC.TI[t].I != nullptr) {
                for (int i = 0; i < SC.TI[t].InstanceCount; i++) {

                    if (SC.TI[t].I[i].id != nullptr) {
                        std::string instId = *(SC.TI[t].I[i].id);

                        // Se l'istanza corrente fa parte degli skydome
                        if (instId.find("skydome") != std::string::npos || instId.find("sky") != std::string::npos) {

                            // Verifica se è esattamente l'istanza attiva del giorno/notte
                            if (instId == activeSkyId) {
                                // Centrato sulla posizione del giocatore + ROTAZIONE DI 180 GRADI sull'asse Y
                                SC.TI[t].I[i].Wm = glm::translate(glm::mat4(1.0f), player.position) *
                                                  glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                                                  glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
                            } else {
                                // Nasconde gli skydome inattivi
                                SC.TI[t].I[i].Wm = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));
                            }
                        }
                    }

                    ubo.mMat = SC.TI[t].I[i].Wm;

                    // PASS 0 (Ombre)
                    ubo.mvpMat = gubo.lightVP * ubo.mMat;
                    SC.TI[t].I[i].DS[0][1]->map((int) currentImage, &ubo, 0);

                    // PASS 1 (Colore)
                    ubo.mvpMat = ViewPrj * ubo.mMat;
                    SC.TI[t].I[i].DS[1][0]->map((int) currentImage, &gubo, 0);
                    SC.TI[t].I[i].DS[1][1]->map((int) currentImage, &ubo, 0);
                }
            }
        }

        // =========================================================
        // UPDATE SKIN PLAYER (Posizione + Animazione)
        // =========================================================

        // 1. Aggiorna la World Matrix del modello applicando rotazione correttiva e scala
        // 1. Visibilità della skin
        auto itPlayer = SC.InstanceIds.find("player");
        if (itPlayer != SC.InstanceIds.end()) {
            if (player.isFixedCamera()) {
                SC.I[itPlayer->second]->Wm = player.getWorldMatrix();
            } else {
                SC.I[itPlayer->second]->Wm = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));
            }
        }

        // 2. Disabilita interazione fisica durante le telecamere fisse
        bool canInteract = !dialogueManager.isDialogueActive() && !player.isFixedCamera();
        physicsManager.update(window, deltaT, SC, player, canInteract, txt);

        // 2. Determina se il giocatore si sta muovendo
        bool isMoving = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

        // 3. Riproduci l'animazione corretta tramite animManager
        // (0 = Camminata/Movimento, 1 = Idle/Fermo, oppure invertili in base al tuo rig)
        static int playerCurrentAnim = -1;
        int targetAnim = isMoving ? 0 : 1;

        if (playerCurrentAnim != targetAnim) {
            npcAnimManager.play("player", targetAnim, 0.2f);
            playerCurrentAnim = targetAnim;
        }

        // Aggiornamento finale dei modelli animati
        npcAnimManager.update(SC, currentImage, gubo, ViewPrj, deltaT);

        // =========================================================
        // UPDATE PHYSICS (Gravità per oggetti dinamici)
        // =========================================================

        canInteract = !dialogueManager.isDialogueActive();
        physicsManager.update(window, deltaT, SC, player, canInteract, txt);

        // Aggiornamento FPS
        static float elapsedT = 0.0f;
        static int countedFrames = 0;

        countedFrames++;
        elapsedT += deltaT;
        if (elapsedT > 1.0f) {
            float Fps = (float) countedFrames / elapsedT;
            elapsedT = 0.0f;
            countedFrames = 0;
        }

        txt.updateCommandBuffer();

        // =========================================================
        // DEBUG: PREMI 'P' PER STAMPARE LA POSIZIONE NELLA CONSOLE
        // =========================================================
        static bool pPressed = false;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pPressed) {
                std::cout << "{\n";
                std::cout << "   \"position\": [" << player.position.x << ", " << player.position.y << ", " << player.position.z << "],\n";
                std::cout << "   \"color\": [1.0, 0.6, 0.2],\n";
                std::cout << "   \"intensity\": 15.0\n";
                std::cout << "},\n";

                pPressed = true;
            }
        } else {
            pPressed = false;
        }
    }
};

// Main Entry Point
int main() {
    DungeonTavern app;

    try {
        app.run(false);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
