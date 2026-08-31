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

// Stato del gioco
enum class GameState { MENU, PLAYING };

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
    // SISTEMA SKYDOME: Gestione del cielo
    // ==========================================

    struct SkydomeRef {
        std::string id;
        Instance* instancePtr;
    };
    std::vector<SkydomeRef> skydomeInstances;

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

    GameState currentState = GameState::MENU;
    int menuSelection = 0; // 0 = Gioca, 1 = Esci
    int textTitleId = -1;
    int textGiocaId = -1;
    int textEsciId  = -1;

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


        DPSZs.uniformBlocksInPool = 100;
        DPSZs.texturesInPool = 150;
        DPSZs.setsInPool = 100;

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

        // Popola la cache degli skydome dopo il caricamento della scena
        skydomeInstances.clear();
        int staticTechniques[] = {0, 2, 3, 4, 5, 6};

        for (int t : staticTechniques) {
            if (t < SC.TechniqueInstanceCount && SC.TI[t].I != nullptr) {
                for (int i = 0; i < SC.TI[t].InstanceCount; i++) {
                    if (SC.TI[t].I[i].id != nullptr) {
                        std::string instId = *(SC.TI[t].I[i].id);
                        if (instId.find("skydome") != std::string::npos || instId.find("sky") != std::string::npos) {
                            skydomeInstances.push_back({instId, &SC.TI[t].I[i]});
                        }
                    }
                }
            }
        }

        // Inizializzazione moduli di base
        txt.init(this, (int) windowWidth, (int) windowHeight);
        player.init(glm::vec3(11.5f, 3.0f, 13.0f), 90.0f, 10.0f, 0.5f);
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
                    {0, 28, 1.0f, 0},
                    {32, 120, 1.0f, 0},
                    {122, 300, 1.0f, 0}
                }
            },
            {
                "player", "assets/models/player/player.gltf", "maleCombined", 0, glm::mat4(1.0f),
                {
                    {0, 134, 1.0f, 0},
                    {135, 165, 1.0f, 0},
                    {166, 184, 1.0f, 0},
                    {185, 249, 1.0f, 0}
                },
            },
        });
        tavernNPCs = TavernNPC::loadNPCsFromJson("assets/scenes/scene.json", SC, npcAnimManager);
        tavernNPCs.erase(std::remove_if(tavernNPCs.begin(), tavernNPCs.end(),
            [](const TavernNPC& npc) { return npc.name == "player"; }), tavernNPCs.end());
        physicsManager.init(SC, "assets/scenes/scene.json", 0.3f);
        lightManager.init(12.0f);
        lightManager.loadLightsFromJson("assets/scenes/scene.json");

        // =====================================================================
        // CONFIGURAZIONE MISSIONI
        // =====================================================================

        missionManager.addCollectionMission(2, "boccale", "tavolo_quadrato1", 3, "Trova i boccali", "Hai raccolto tutti i boccali!", 10.0f, 10.0f);
        missionManager.addCollectionMission(4, "piatto", "tavolo_quadrato1", 5, "Trova i piatti", "Hai raccolto tutti i piatti!", 10.0f, 10.0f);

        // =====================================================================
        // CONFIGURAZIONE TRIGGER
        // =====================================================================
        triggerPorta.initAABB(12.5643, -0.363361, -6.37082, 9.56568, 4.13063, -7.6541);
        triggerPorta.setWorldMatrix(glm::mat4(1.0f));

        // NON TOGLIERE: Trucco anti-crash per il buffer vuoto del TextMaker
        txt.print(-100.0f, -100.0f, " ");

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsInit() override {
        //texture depth 2048x2048
        RPshadow.init(this, 2048, 2048, 1, RenderPass::getStandardAttchmentsProperties(AT_DEPTH_ONLY, this), RenderPass::getStandardDependencies(ATDEP_NO_DEP), true);
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

        // --- GESTIONE INPUT DEL MENU ---
        static bool upPressedLastFrame = false;
        static bool downPressedLastFrame = false;
        static bool enterPressedLastFrame = false;

        bool upPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
        bool downPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
        bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

        glm::vec3 oldPlayerPos = player.position;

        if (currentState == GameState::MENU) {

            // 1.1 Disegna il Titolo
            if (textTitleId == -1) {
                textTitleId = txt.print(0.0f, -0.4f, "DUNGEON TAVERN", textTitleId, "SS", false, true, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE, glm::vec4(1.0f, 0.7f, 0.1f, 1.0f), glm::vec4(0), glm::vec4(0,0,0,0.8f), 2.0f, 2.0f);
            }

            // 1.2 Navigazione del Menu
            if (upPressed && !upPressedLastFrame) menuSelection = 0;
            if (downPressed && !downPressedLastFrame) menuSelection = 1;

            // 1.3 Disegna le Opzioni
            std::string strGioca = (menuSelection == 0) ? "> GIOCA <" : "  GIOCA  ";
            std::string strEsci  = (menuSelection == 1) ? "> ESCI <"  : "  ESCI  ";

            glm::vec4 colorGioca = (menuSelection == 0) ? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(0.8f);
            glm::vec4 colorEsci  = (menuSelection == 1) ? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(0.8f);

            textGiocaId = txt.print(0.0f, 0.1f, strGioca, textGiocaId, "SS", false, true, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE, colorGioca);
            textEsciId  = txt.print(0.0f, 0.3f, strEsci, textEsciId, "SS", false, true, false, TAL_CENTER, TRH_CENTER, TRV_MIDDLE, colorEsci);

            // 1.4 Selezione
            if (enterPressed && !enterPressedLastFrame) {
                if (menuSelection == 0) {
                    // TRANSIZIONE A PLAYING
                    currentState = GameState::PLAYING;

                    // Pulisce l'interfaccia del menu
                    txt.removeText(textTitleId); textTitleId = -1;
                    txt.removeText(textGiocaId); textGiocaId = -1;
                    txt.removeText(textEsciId);  textEsciId  = -1;

                    // Lancia il boccale iniziale!
                    glm::vec3 throwPos = player.position + player.getForwardVector() * 1.0f;
                    throwPos.y += 0.5f;
                    physicsManager.throwObject(SC, "boccale_start", throwPos, player.getForwardVector() * 15.0f + glm::vec3(0, 3.0f, 0));

                } else if (menuSelection == 1) {
                    // USCITA DAL GIOCO
                    glfwSetWindowShouldClose(window, GL_TRUE);
                }
            }

        } else if (currentState == GameState::PLAYING) {

            // --- LOGICA DI GIOCO ATTIVA SOLO IN PLAYING ---
            dialogueManager.update(window, deltaT, player, tavernNPCs, txt, windowWidth);

            if (!dialogueManager.isDialogueActive()) {
                player.processInput(window, deltaT, SC, physicsManager);
            }

            missionManager.update(deltaT, dialogueManager, SC, physicsManager, txt);

            // GESTIONE OBIETTIVI UI
            static int objTextId = -1;
            std::string objStr = "";
            int prog = dialogueManager.getStoryProgress();

            if (prog == 0) objStr = "Obiettivo: Esci dalla taverna";
            else if (prog == 1) objStr = "Obiettivo: Parla con l'oste";
            else if (prog == 3) objStr = "Obiettivo: Torna dall'oste";
            else if (prog == 5) objStr = "Obiettivo: Torna dall'oste";
            else if (prog >= 7) objStr = "Demo terminata";

            if (!objStr.empty()) {
                objTextId = txt.print(-0.95f, -0.9f, objStr, objTextId, "SS", false, false, false, TAL_LEFT, TRH_LEFT, TRV_TOP, glm::vec4(1.0f));
            } else if (objTextId != -1) {
                txt.removeText(objTextId);
                objTextId = -1;
            }

            // CONTROLLO TRIGGER PORTA
            if (prog < 6) {
                if (player.playerCollider && triggerPorta.collidesWith(*(player.playerCollider))) {
                    if (!triggerPortaAttivato && !dialogueManager.isDialogueActive()) {
                        dialogueManager.forceStartDialogue("door_guard_r", tavernNPCs, txt, player);
                        triggerPortaAttivato = true;
                    }
                    player.position = oldPlayerPos;
                } else {
                    triggerPortaAttivato = false;
                }
            }
            if (prog == 6) {
                if (player.playerCollider && triggerPorta.collidesWith(*(player.playerCollider))) {
                    dialogueManager.setStoryProgress(7);
                }
            }
        }

        // Salva l'input per il frame successivo
        upPressedLastFrame = upPressed;
        downPressedLastFrame = downPressed;
        enterPressedLastFrame = enterPressed;


        // =========================================================
        // AGGIORNAMENTI GLOBALI (Attivi sia nel Menu che in Gioco)
        // =========================================================

        int talkingNPC = dialogueManager.getDialogueNPC();
        for (size_t i = 0; i < tavernNPCs.size(); ++i) {
            bool isTalking = (currentState == GameState::PLAYING && i == talkingNPC);
            tavernNPCs[i].update(deltaT, isTalking, player.position, npcAnimManager, SC);
        }

        lightManager.update(deltaT, window);

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

       // =========================================================
        // 1. GESTIONE SKYDOME (Attivo/Inattivo)
        // =========================================================
        std::string activeSkyId = lightManager.getCurrentSkydomeInstanceId();

        for (auto& skyRef : skydomeInstances) {
            if (skyRef.id == activeSkyId) {
                // Skydome attivo: centrato sul player e visibile
                skyRef.instancePtr->Wm = glm::translate(glm::mat4(1.0f), player.position) *
                                         glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                                         glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
            } else {
                // Skydome inattivo: sposta lontano sotto la mappa (evita matrici con scala 0)
                skyRef.instancePtr->Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -10000.0f, 0.0f));
            }
        }

        // =========================================================
        // 2. AGGIORNA MATERIALI STATICI (Tecniche 0, 2, 3, 4, 5, 6)
        // =========================================================
        int staticTechniques[] = {0, 2, 3, 4, 5, 6};
        float renderDistance = 70.0f;
        // --- DEFINIZIONE LIMITI BOX TAVERNA (3D: X, Y, Z) ---
        const float minX = 1.11968f;
        const float maxX = 19.5197f;

        const float minY = -1.0f;
        const float maxY = 16.0f;

        const float minZ = -5.76789f;
        const float maxZ = 21.0792f;

        bool isInTavernRoom = (player.position.x >= minX && player.position.x <= maxX) &&
                              (player.position.y >= minY && player.position.y <= maxY) &&
                              (player.position.z >= minZ && player.position.z <= maxZ);

        for (int t : staticTechniques) {
            if (t < SC.TechniqueInstanceCount && SC.TI[t].I != nullptr) {
                for (int i = 0; i < SC.TI[t].InstanceCount; i++) {

                    glm::vec3 objPos = glm::vec3(SC.TI[t].I[i].Wm[3]);
                    bool isVisible = false;

                    // Gli skydome (tecnica 6) seguono sempre il giocatore
                    if (t == 6) {
                        isVisible = true;
                    } else {
                        // Verifica se l'oggetto è vicino al giocatore
                        if (glm::distance(player.position, objPos) < renderDistance || !isInTavernRoom) {
                            isVisible = true;
                        } else {
                            // Salva dalla sparizione gli oggetti giganti (pavimenti e montagne)
                            // i cui centri potrebbero trovarsi molto distanti
                            if (SC.TI[t].I[i].id != nullptr) {
                                std::string objName = *(SC.TI[t].I[i].id);
                                if (objName.find("floor") != std::string::npos ||
                                    objName.find("mountain") != std::string::npos ||
                                    objName.find("village_building") != std::string::npos ||
                                    objName.find("barrel") != std::string::npos ||
                                    objName.find("well") != std::string::npos ||
                                    objName.find("street_lamp") != std::string::npos ||
                                    objName.find("muro") != std::string::npos) {
                                    isVisible = true;
                                }
                            }
                        }
                    }

                    // Se è visibile usiamo la sua matrice normale, altrimenti lo collassiamo
                    if (isVisible) {
                        ubo.mMat = SC.TI[t].I[i].Wm;
                    } else {
                        ubo.mMat = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));
                    }

                    // PASS 0 (Ombre): Salta il pass ombre se è uno Skydome (Tecnica 6)
                    if (t != 6) {
                        ubo.mvpMat = gubo.lightVP * ubo.mMat;
                        SC.TI[t].I[i].DS[0][1]->map((int) currentImage, &ubo, 0);
                    }

                    // PASS 1 (Colore/Schermo)
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
        auto itPlayer = SC.InstanceIds.find("player");
        if (itPlayer != SC.InstanceIds.end()) {
            if (player.isFixedCamera()) {
                SC.I[itPlayer->second]->Wm = player.getWorldMatrix();
            } else {
                SC.I[itPlayer->second]->Wm = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));
            }
        }

        // 2. Determina lo stato degli input del giocatore
        bool isMoving = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);
        bool isRunning = isMoving && (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
        bool isJumping = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

        // 3. Riproduci l'animazione corretta tramite animManager
        static int playerCurrentAnim = -1;
        int targetAnim = 0; // Animazione 0: Idle (Fermo)

        if (isJumping) {
            targetAnim = 3; // Animazione 3: Salto
        } else if (isRunning) {
            targetAnim = 2; // Animazione 2: Corsa
        } else if (isMoving) {
            targetAnim = 1; // Animazione 1: Camminata
        }

        // Applica il cambio di animazione solo se lo stato è effettivamente cambiato
        if (playerCurrentAnim != targetAnim) {
            npcAnimManager.play("player", targetAnim, 0.2f);
            playerCurrentAnim = targetAnim;
        }

        bool canInteract = !dialogueManager.isDialogueActive() && !player.isFixedCamera();
        physicsManager.update(window, deltaT, SC, player, canInteract, txt);

        // Aggiornamento finale dei modelli animati
        npcAnimManager.update(SC, currentImage, gubo, ViewPrj, deltaT);

        // =========================================================
        // UPDATE PHYSICS (Gravità per oggetti dinamici)
        // =========================================================


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
