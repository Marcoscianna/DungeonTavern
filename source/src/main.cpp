// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <stb_image.h>

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

// Vertice per modelli animati
struct VertexAnim {
    glm::vec3 pos;
    glm::vec2 UV;
    glm::vec3 norm;
    glm::vec4 jointWeights;
    glm::uvec4 jointIndices;
};

enum class GameState { MENU, PLAYING };

// Main app
class DungeonTavern : public BaseProject {
protected:
    // Descriptor Layouts
    DescriptorSetLayout DSLlocal, DSLglobal, DSLemissive;

    // Animazioni
    DescriptorSetLayout DSLanim;
    VertexDescriptor VDanim;
    Pipeline Panim;

    // Vertex formats, Pipelines and Render passes
    VertexDescriptor VD;
    RenderPass RP;
    Pipeline P, Pemissive, Pwood, Pstone, Pmetal, Psky;

    // Ombre
    RenderPass RPshadow;
    Pipeline Pshadow, PanimShadow;
    bool shadowsEnabled = true;
    bool lPressedLastFrame = false;

    // Models, textures and Descriptors
    DescriptorSet DSglobal;

    // Supporto per il caricamento asset dal file scene.json
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef> PRs;

    // Feedback testuale a schermo
    TextMaker txt;

    float Ar; // Aspect ratio

    // Gestione player (movimento/visuale)
    Player player;

    // Gestione fisica
    PhysicsManager physicsManager;

    // Gestione ciclo giorno/notte e luci
    LightManager lightManager;

    // ==========================================
    // Skydome
    // ==========================================

    struct SkydomeRef {
        std::string id;
        Instance *instancePtr;
    };

    std::vector<SkydomeRef> skydomeInstances;

    // ==========================================
    // Setup NPC
    // ==========================================

    // Logica e collisioni NPC
    std::vector<TavernNPC> tavernNPCs;

    // Manager animazioni 3D
    AnimatedNPCRig npcAnimManager;

    // Manager per UI e logica dei dialoghi
    DialogueManager dialogueManager;

    MissionManager missionManager;

    // Trigger porta
    Collider triggerPorta;
    bool triggerPortaAttivato = false;

    GameState currentState = GameState::MENU;
    int menuSelection = 0; // 0 = Gioca, 1 = Esci
    int textTitleId = -1;
    int textGiocaId = -1;
    int textEsciId = -1;

    // ID UI per la legenda del menu
    int textControlsKbdHeaderId = -1;
    int textControlsKbdLinesId = -1;
    int textControlsPadHeaderId = -1;
    int textControlsPadLinesId = -1;

public:
    DungeonTavern() : Ar(4.0f / 3.0f) {
    }

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
        GLFWimage images[1];
        images[0].pixels = stbi_load("assets/textures/icon.png", &images[0].width, &images[0].height, 0, 4);

        if (images[0].pixels) {
            glfwSetWindowIcon(window, 1, images);
            stbi_image_free(images[0].pixels);
        } else {
            std::cout << "Warning: Impossibile caricare l'icona. Motivo: " << stbi_failure_reason() << "\n";
        }

        // Layout per UBO Animato
        DSLanim.init(this, {
                         {
                             0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,
                             sizeof(AnimUniformBufferObject), 1
                         },
                         {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}, // Diffuse
                         {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}, // Normal
                         {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1} // Specular
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
        //Pemissive.setCullMode(VK_CULL_MODE_NONE);

        Psky.init(this, &VD, "shaders/static.vert.spv",
                  "shaders/sky.frag.spv",
                  {&DSLglobal, &DSLemissive});

        Pwood.init(this, &VD, "shaders/static.vert.spv", "shaders/wood.frag.spv", {&DSLglobal, &DSLlocal});
        Pstone.init(this, &VD, "shaders/static.vert.spv", "shaders/stone.frag.spv", {&DSLglobal, &DSLlocal});
        Pmetal.init(this, &VD, "shaders/static.vert.spv", "shaders/metal.frag.spv", {&DSLglobal, &DSLlocal});

        Pshadow.init(this, &VD, "shaders/shadow.vert.spv", "shaders/shadow.frag.spv", {&DSLglobal, &DSLlocal});
        //Pshadow.setCullMode(VK_CULL_MODE_NONE);
        PanimShadow.init(this, &VDanim, "shaders/shadow_anim.vert.spv", "shaders/shadow.frag.spv",
                         {&DSLglobal, &DSLanim});
        //PanimShadow.setCullMode(VK_CULL_MODE_NONE);


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

        // Cache skydome
        skydomeInstances.clear();
        int staticTechniques[] = {0, 2, 3, 4, 5, 6};

        for (int t: staticTechniques) {
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
        player.init(glm::vec3(12.0f, 3.0f, 13.0f), 90.0f, 10.0f, 0.5f);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // =====================================================================
        // Configurazione Manager
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
            {
                "archer_npc", "assets/models/npc/archer/archer.gltf", "mixamo.com", 0, glm::mat4(1.0f),
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
                                        [](const TavernNPC &npc) { return npc.name == "player"; }), tavernNPCs.end());
        physicsManager.init(SC, "assets/scenes/scene.json", 0.3f);
        lightManager.init(12.0f);
        lightManager.loadLightsFromJson("assets/scenes/scene.json");

        // =====================================================================
        // Configurazione Missioni
        // =====================================================================

        missionManager.addCollectionMission(2, "boccale", "tavolo_quadrato1", 3, "Trova i boccali",
                                            "Hai raccolto tutti i boccali!", 10.0f, 10.0f);
        missionManager.addCollectionMission(4, "piatto", "tavolo_quadrato1", 5, "Trova i piatti",
                                            "Hai raccolto tutti i piatti!", 10.0f, 10.0f);

        // =====================================================================
        // Configurazione Trigger
        // =====================================================================
        triggerPorta.initAABB(12.5643, -0.363361, -6.37082, 9.56568, 4.13063, -7.6541);
        triggerPorta.setWorldMatrix(glm::mat4(1.0f));

        // Anti-crash per buffer vuoto TextMaker
        txt.print(-100.0f, -100.0f, " ");

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsInit() override {
        // Shadow map depth
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

        // Setup shadow map nelle techniques
        TextureDefs shadowTexDef = {false, 0, RPshadow.attachments[0].getViewAndSampler()};
        for (int t = 0; t < 7; t++) {
            for (int p = 0; p < 2; p++) {
                PRs[t].PT[p].texDefs[0].clear();
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
        // Pass 0: Ombre (sole)
        RPshadow.begin(commandBuffer, 0);
        SC.populateCommandBuffer(commandBuffer, 0, currentImage);
        RPshadow.end(commandBuffer);

        // Pass 1: Scena principale
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

        // =========================================================
        // Input Gamepad
        // =========================================================
        GLFWgamepadstate gamepadState;
        bool hasGamepad = glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

        // =========================================================
        // TOGGLE FULLSCREEN (Tasto R o Tasto BACK sul Gamepad)
        // =========================================================
        static bool rPressedLastFrame = false;
        bool rPressed = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) ||
                        (hasGamepad && gamepadState.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS);

        if (rPressed && !rPressedLastFrame) {
            static int savedX = 100, savedY = 100, savedWidth = 800, savedHeight = 600;
            static bool isFullscreen = false; // Usiamo una variabile locale indipendente

            GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

            if (!isFullscreen) {
                // Salva posizione e dimensioni attuali
                glfwGetWindowPos(window, &savedX, &savedY);
                glfwGetWindowSize(window, &savedWidth, &savedHeight);

                // Modalità "Borderless Windowed"
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE); // Rimuove la cornice
                glfwSetWindowPos(window, 0, 0); // Sposta in alto a sinistra
                glfwSetWindowSize(window, mode->width, mode->height); // Copre tutto il monitor

                isFullscreen = true;
            } else {
                // Ripristina la finestra normale
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE); // Ripristina la cornice
                glfwSetWindowPos(window, savedX, savedY);
                glfwSetWindowSize(window, savedWidth, savedHeight);

                isFullscreen = false;
            }
        }
        rPressedLastFrame = rPressed;

        auto applyDeadzone = [](float value, float threshold = 0.15f) -> float {
            if (std::abs(value) < threshold) return 0.0f;
            return value;
        };

        // --- Toggle Ombre ---
        bool gamepadL = hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS);
        bool lPressed = (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) || gamepadL;
        if (lPressed && !lPressedLastFrame) {
            shadowsEnabled = !shadowsEnabled;
        }
        lPressedLastFrame = lPressed;

        // =========================================================
        // 1. Update Game Logic (Dialoghi, Input, Collisioni)
        // =========================================================

        static bool upPressedLastFrame = false;
        static bool downPressedLastFrame = false;
        static bool enterPressedLastFrame = false;

        // Input menu
        bool kbdUp = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
        bool kbdDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
        bool kbdEnter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_E) ==
                        GLFW_PRESS;

        bool gamepadUp = hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                                        applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) < -0.5f);
        bool gamepadDown = hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                                          applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) > 0.5f);
        bool gamepadEnter = hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS ||
                                           gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);

        bool upPressed = kbdUp || gamepadUp;
        bool downPressed = kbdDown || gamepadDown;
        bool enterPressed = kbdEnter || gamepadEnter;

        glm::vec3 oldPlayerPos = player.position;

        if (currentState == GameState::MENU) {
            // Titolo Menu
            if (textTitleId == -1) {
                textTitleId = txt.print(0.0f, -0.65f, "DUNGEON TAVERN", textTitleId, "SS", false, true, false,
                                        TAL_CENTER, TRH_CENTER, TRV_MIDDLE, glm::vec4(1.0f, 0.7f, 0.1f, 1.0f),
                                        glm::vec4(0), glm::vec4(0, 0, 0, 0.8f), 2.0f, 2.0f);
            }

            // Navigazione
            if (upPressed && !upPressedLastFrame) menuSelection = 0;
            if (downPressed && !downPressedLastFrame) menuSelection = 1;

            std::string strGioca = (menuSelection == 0) ? "> GIOCA <" : "  GIOCA  ";
            std::string strEsci = (menuSelection == 1) ? "> ESCI <" : "  ESCI  ";

            glm::vec4 colorGioca = (menuSelection == 0) ? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(0.8f);
            glm::vec4 colorEsci = (menuSelection == 1) ? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(0.8f);

            textGiocaId = txt.print(0.0f, -0.35f, strGioca, textGiocaId, "SS", false, true, false, TAL_CENTER,
                                    TRH_CENTER, TRV_MIDDLE, colorGioca);
            textEsciId = txt.print(0.0f, -0.20f, strEsci, textEsciId, "SS", false, true, false, TAL_CENTER, TRH_CENTER,
                                   TRV_MIDDLE, colorEsci);

            // UI Legenda comandi - Colonna SX
            if (textControlsKbdHeaderId == -1) {
                textControlsKbdHeaderId = txt.print(-0.55f, 0.05f, "CONTROLLI TASTIERA", textControlsKbdHeaderId, "SS",
                                                    false, true, false, TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                    glm::vec4(0.3f, 0.9f, 1.0f, 1.0f));
            }
            std::string kbdText =
                    "SHIFT : Corsa\n"
                    "SPAZIO : Salto\n"
                    "E : Interagisci / Dialogo\n"
                    "Q : Prendi / Lascia Oggetti\n"
                    "M : Vola\n"
                    "C : Telecamera 1a persona\n"
                    "X : Telecamera 3a persona";
            textControlsKbdLinesId = txt.print(-0.55f, 0.15f, kbdText, textControlsKbdLinesId, "SS", false, false,
                                               false, TAL_CENTER, TRH_CENTER, TRV_TOP, glm::vec4(0.9f));

            // UI Legenda comandi - Colonna DX
            if (textControlsPadHeaderId == -1) {
                textControlsPadHeaderId = txt.print(0.55f, 0.05f, "CONTROLLI GAMEPAD", textControlsPadHeaderId, "SS",
                                                    false, true, false, TAL_CENTER, TRH_CENTER, TRV_TOP,
                                                    glm::vec4(0.3f, 0.9f, 1.0f, 1.0f));
            }
            std::string padText =
                    "L3 / RT : Corsa\n"
                    "Tasto A : Salto\n"
                    "Tasto A : Interagisci / Dialogo\n"
                    "Tasto X : Prendi / Lascia Oggetti\n"
                    "Tasto Y : Vola\n"
                    "Freccia dx : Telecamera\n"
                    "Freccia sx : Telecamera 3a persona";
            textControlsPadLinesId = txt.print(0.55f, 0.15f, padText, textControlsPadLinesId, "SS", false, false, false,
                                               TAL_CENTER, TRH_CENTER, TRV_TOP, glm::vec4(0.9f));

            // Invio selezione
            if (enterPressed && !enterPressedLastFrame) {
                if (menuSelection == 0) {
                    currentState = GameState::PLAYING;

                    // Cleanup UI
                    txt.removeText(textTitleId);
                    textTitleId = -1;
                    txt.removeText(textGiocaId);
                    textGiocaId = -1;
                    txt.removeText(textEsciId);
                    textEsciId = -1;
                    txt.removeText(textControlsKbdHeaderId);
                    textControlsKbdHeaderId = -1;
                    txt.removeText(textControlsKbdLinesId);
                    textControlsKbdLinesId = -1;
                    txt.removeText(textControlsPadHeaderId);
                    textControlsPadHeaderId = -1;
                    txt.removeText(textControlsPadLinesId);
                    textControlsPadLinesId = -1;

                    // Lancia l'oggetto iniziale
                    glm::vec3 throwPos = player.position + player.getForwardVector() * 1.0f;
                    throwPos.y += 0.5f;
                    physicsManager.throwObject(SC, "boccale_start", throwPos,
                                               player.getForwardVector() * 15.0f + glm::vec3(0, 3.0f, 0));
                } else if (menuSelection == 1) {
                    glfwSetWindowShouldClose(window, GL_TRUE);
                }
            }
        } else if (currentState == GameState::PLAYING) {
            dialogueManager.update(window, deltaT, player, tavernNPCs, txt, windowWidth);

            if (!dialogueManager.isDialogueActive()) {
                player.processInput(window, deltaT, SC, physicsManager);
            }

            missionManager.update(deltaT, dialogueManager, SC, physicsManager, txt);

            // UI Testo obiettivi
            static int objTextId = -1;
            static std::string lastObjStr = "";
            std::string objStr = "";
            int prog = dialogueManager.getStoryProgress();

            if (prog == 0 || prog == 6) objStr = "Obiettivo: Esci dalla taverna";
            else if (prog == 1) objStr = "Obiettivo: Parla con l'oste";
            else if (prog == 3) objStr = "Obiettivo: Torna dall'oste";
            else if (prog == 5) objStr = "Obiettivo: Torna dall'oste";
            else if (prog >= 7) objStr = "Demo terminata";

            if (objStr != lastObjStr) {
                if (!objStr.empty()) {
                    objTextId = txt.print(-0.95f, -0.9f, objStr, objTextId, "SS", false, false, false, TAL_LEFT,
                                          TRH_LEFT, TRV_TOP, glm::vec4(1.0f));
                } else if (objTextId != -1) {
                    txt.removeText(objTextId);
                    objTextId = -1;
                }
                lastObjStr = objStr;
            }

            // Controllo trigger porta
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

        // Setup input prev frame
        upPressedLastFrame = upPressed;
        downPressedLastFrame = downPressed;
        enterPressedLastFrame = enterPressed;

        // =========================================================
        // Aggiornamenti globali
        // =========================================================

        int talkingNPC = dialogueManager.getDialogueNPC();
        for (size_t i = 0; i < tavernNPCs.size(); ++i) {
            bool isTalking = (currentState == GameState::PLAYING && i == talkingNPC);
            tavernNPCs[i].update(deltaT, isTalking, player.position, npcAnimManager, SC);
        }

        lightManager.update(deltaT, window);

        // =========================================================
        // 2. Update Graphics (Matrici, Uniforms, Rendering)
        // =========================================================

        glm::mat4 ViewPrj = player.getViewProjectionMatrix(Ar);
        SC.updateColliderVisualizer(currentImage, ViewPrj);

        // Telecamera ombra direzionale
        GlobalUniformBufferObject gubo{};
        gubo.eyePos = player.position;
        gubo.shadowToggle = shadowsEnabled ? 1.0f : 0.0f;
        lightManager.applyToGUBO(gubo);

        glm::mat4 lightProj = glm::ortho(-50.0f, 50.0f, -150.0f, 150.0f, -50.0f, 100.0f);
        lightProj[1][1] *= -1;

        glm::vec3 tavernCenter = glm::vec3(11.0f, 0.0f, -25.0f);
        glm::vec3 lightPos = tavernCenter - gubo.lightDir * 50.0f;
        glm::mat4 lightViewMat = glm::lookAt(lightPos, tavernCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        gubo.lightVP = lightProj * lightViewMat;
        DSglobal.map((int) currentImage, &gubo, 0);

        UniformBufferObject ubo{};

        // Gestione Skydome
        std::string activeSkyId = lightManager.getCurrentSkydomeInstanceId();

        for (auto &skyRef: skydomeInstances) {
            if (skyRef.id == activeSkyId) {
                skyRef.instancePtr->Wm = glm::translate(glm::mat4(1.0f), player.position) *
                                         glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                                     glm::vec3(1.0f, 0.0f, 0.0f)) *
                                         glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
            } else {
                skyRef.instancePtr->Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -10000.0f, 0.0f));
            }
        }

        // Setup materiali statici e Frustum/Distance Culling
        int staticTechniques[] = {0, 2, 3, 4, 5, 6};
        float renderDistance = 70.0f;
        float renderDistSq = renderDistance * renderDistance;

        const float minX = 1.11968f;
        const float maxX = 19.5197f;
        const float minY = -1.0f;
        const float maxY = 16.0f;
        const float minZ = -5.76789f;
        const float maxZ = 21.0792f;

        bool isInTavernRoom = (player.position.x >= minX && player.position.x <= maxX) &&
                              (player.position.y >= minY && player.position.y <= maxY) &&
                              (player.position.z >= minZ && player.position.z <= maxZ);

        static bool cullingInitialized = false;
        static std::vector<std::vector<bool> > isLargeObject(7);

        if (!cullingInitialized) {
            for (int t: staticTechniques) {
                if (t < SC.TechniqueInstanceCount && SC.TI[t].I != nullptr) {
                    isLargeObject[t].resize(SC.TI[t].InstanceCount, false);
                    for (int i = 0; i < SC.TI[t].InstanceCount; i++) {
                        if (SC.TI[t].I[i].id != nullptr) {
                            const std::string &objName = *(SC.TI[t].I[i].id);
                            if (objName.find("floor") != std::string::npos ||
                                objName.find("mountain") != std::string::npos ||
                                objName.find("village_building") != std::string::npos ||
                                objName.find("barrel") != std::string::npos ||
                                objName.find("well") != std::string::npos ||
                                objName.find("street_lamp") != std::string::npos ||
                                objName.find("muro") != std::string::npos) {
                                isLargeObject[t][i] = true;
                            }
                        }
                    }
                }
            }
            cullingInitialized = true;
        }

        static const glm::mat4 zeroMat = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));

        // ====================================================================
        // Render Loop
        // ====================================================================
        for (int t: staticTechniques) {
            if (t < SC.TechniqueInstanceCount && SC.TI[t].I != nullptr) {
                for (int i = 0; i < SC.TI[t].InstanceCount; i++) {
                    bool isVisible = false;

                    if (t == 6 || !isInTavernRoom || isLargeObject[t][i]) {
                        isVisible = true;
                    } else {
                        glm::vec3 objPos = glm::vec3(SC.TI[t].I[i].Wm[3]);
                        glm::vec3 diff = player.position - objPos;
                        float distSq = (diff.x * diff.x) + (diff.y * diff.y) + (diff.z * diff.z);

                        if (distSq < renderDistSq) {
                            isVisible = true;
                        }
                    }

                    if (isVisible) {
                        ubo.mMat = SC.TI[t].I[i].Wm;

                        if (t != 6) {
                            ubo.mvpMat = gubo.lightVP * ubo.mMat;
                            SC.TI[t].I[i].DS[0][1]->map((int) currentImage, &ubo, 0);
                        }

                        ubo.mvpMat = ViewPrj * ubo.mMat;
                        SC.TI[t].I[i].DS[1][0]->map((int) currentImage, &gubo, 0);
                        SC.TI[t].I[i].DS[1][1]->map((int) currentImage, &ubo, 0);
                    } else {
                        ubo.mMat = zeroMat;
                        ubo.mvpMat = zeroMat;

                        if (t != 6) SC.TI[t].I[i].DS[0][1]->map((int) currentImage, &ubo, 0);
                        SC.TI[t].I[i].DS[1][0]->map((int) currentImage, &gubo, 0);
                        SC.TI[t].I[i].DS[1][1]->map((int) currentImage, &ubo, 0);
                    }
                }
            }
        }

        // =========================================================
        // Player e Animazioni
        // =========================================================

        auto itPlayer = SC.InstanceIds.find("player");
        if (itPlayer != SC.InstanceIds.end()) {
            if (player.isFixedCamera() || player.getisThirdPerson()) {
                SC.I[itPlayer->second]->Wm = player.getWorldMatrix();
            } else {
                SC.I[itPlayer->second]->Wm = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f));
            }
        }

        // Rilevamento stato di movimento e input controller
        float leftStickX = hasGamepad ? applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X]) : 0.0f;
        float leftStickY = hasGamepad ? applyDeadzone(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) : 0.0f;

        bool isMovingKbd = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
                           (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) ||
                           (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) ||
                           (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

        bool isMovingGamepad = (std::abs(leftStickX) > 0.0f || std::abs(leftStickY) > 0.0f);
        bool isMoving = isMovingKbd || isMovingGamepad;

        bool isRunningKbd = isMoving && (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
        bool isRunningGamepad = isMoving && hasGamepad && (gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f ||
                                                           gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] ==
                                                           GLFW_PRESS);
        bool isRunning = isRunningKbd || isRunningGamepad;

        bool isJumpingKbd = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        bool isJumpingGamepad = hasGamepad && (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);
        bool isJumping = isJumpingKbd || isJumpingGamepad;

        static int playerCurrentAnim = -1;
        int targetAnim = 0; // 0: Idle

        if (isJumping) {
            targetAnim = 3; // 3: Salto
        } else if (isRunning) {
            targetAnim = 2; // 2: Corsa
        } else if (isMoving) {
            targetAnim = 1; // 1: Camminata
        }

        if (playerCurrentAnim != targetAnim) {
            npcAnimManager.play("player", targetAnim, 0.2f);
            playerCurrentAnim = targetAnim;
        }

        bool canInteract = !dialogueManager.isDialogueActive() && !player.isFixedCamera();
        physicsManager.update(window, deltaT, SC, player, canInteract, txt);

        // Update npc animati
        npcAnimManager.update(SC, currentImage, gubo, ViewPrj, deltaT);

        txt.updateCommandBuffer();

        // Debug utils
        /*static bool pPressed = false;
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
        }*/
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
