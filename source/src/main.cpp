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

struct UniformBufferObject {
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
};

struct GlobalUniformBufferObject {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 eyePos;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 UV;
};

// Il vertice per i modelli animati
struct VertexAnim {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec2 UV;
    alignas(16) glm::vec3 norm;
    alignas(16) glm::vec4 jointWeights;
    alignas(16) glm::uvec4 jointIndices;
};

// MAIN !
class DungeonTavern : public BaseProject {
protected:
    // Descriptor Layouts [what will be passed to the shaders]
    DescriptorSetLayout DSLlocal, DSLglobal;

    // NUOVI OGGETTI PER LE ANIMAZIONI
    DescriptorSetLayout DSLanim;
    VertexDescriptor VDanim;
    Pipeline Panim;

    // Vertex formants, Pipelines [Shader couples] and Render passes
    VertexDescriptor VD;
    RenderPass RP;
    Pipeline P;

    // Models, textures and Descriptors (values assigned to the uniforms)
    DescriptorSet DSglobal;

    // To support loading assets from a scene.json file
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef> PRs;

    // to provide textual feedback
    TextMaker txt;

    // Other application parameters
    float Ar;  // Aspect ratio

    // Oggetto Player per gestire movimento e visuale
    Player player;

    // ==========================================
    // SISTEMA NPC: Logica e Grafica
    // ==========================================

    // 1. Logica: Lista degli NPC per gestire collisioni e dialoghi
    std::vector<TavernNPC> tavernNPCs;

    // 2. Grafica: Gestore (Manager) dei modelli 3D animati
    AnimatedNPCRig npcAnimManager;

    // 3. Manager dei Dialoghi: Gestisce stato, UI e interazioni
    DialogueManager dialogueManager;

public:
    DungeonTavern() : Ar(4.0f / 3.0f) {} // Costruttore molto più pulito ora

    void setWindowParameters() override {
        windowWidth = 800;
        windowHeight = 600;
        windowTitle = "Dungeon Tavern";
        windowResizable = GLFW_TRUE;
        Ar = 4.0f / 3.0f;
    }

    void onWindowResize(int w, int h) override {
        std::cout << "Window resized to: " << w << " x " << h << "\n";
        Ar = (float)w / (float)h;
        RP.width = w;
        RP.height = h;
        txt.resizeScreen((int)w, (int)h);
    }

    void localInit() override {
        // 1. Crea il Layout per l'UBO Animato
        DSLanim.init(this, {
              {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(AnimUniformBufferObject), 1},
              {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
        });

        DSLlocal.init(this, {
                 {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObject), 1},
                 {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
                });

        DSLglobal.init(this, {
                 {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(GlobalUniformBufferObject), 1}
                });

        VD.init(this, {
                {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
              }, {
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos), sizeof(glm::vec3), POSITION},
                {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV), sizeof(glm::vec2), UV}
              });

        VDanim.init(this, {
              {0, sizeof(VertexAnim), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
             {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexAnim, pos), sizeof(glm::vec3), POSITION},
             {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexAnim, UV), sizeof(glm::vec2), UV},
             {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexAnim, norm), sizeof(glm::vec3), NORMAL},
             {0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexAnim, jointWeights), sizeof(glm::vec4), JOINTWEIGHT},
             {0, 4, VK_FORMAT_R32G32B32A32_UINT, offsetof(VertexAnim, jointIndices), sizeof(glm::uvec4), JOINTINDEX}
        });

        Panim.init(this, &VDanim,
                 "shaders/skinning.vert.spv",
                 "shaders/toChangeBlinnFromPos.frag.spv",
                 {&DSLglobal, &DSLanim});

        RP.init(this);
        RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};

        P.init(this, &VD, "shaders/toChangeSimplePos.vert.spv",
                      "shaders/toChangeBlinnFromPos.frag.spv",
                      {&DSLglobal, &DSLlocal});

        DPSZs.uniformBlocksInPool = 20;
        DPSZs.texturesInPool = 10;
        DPSZs.setsInPool = 20;

        VDRs.resize(2);
        VDRs[0].init("VDposUV",  &VD);
        VDRs[1].init("VDanim",   &VDanim);

        PRs.resize(2);
        PRs[0].init("BlinnPos", {
                       {&P, {
                        /*DSLglobal*/{},
                        /*DSLlocal*/{ /*t0*/{true,  0, {}} }
                       }}
                      }, 1, &VD);

        PRs[1].init("AnimTech", {
              {&Panim, {
                    /*DSLglobal*/{},
                    /*DSLanim*/{ /*t0*/{true,  0, {}} }
              }}
        }, 1, &VDanim);

        if(SC.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0) {
            std::cout << "ERROR LOADING THE SCENE\n";
            exit(0);
        }

        // Inizializzazione moduli di base
        txt.init(this, (int)windowWidth, (int)windowHeight);
        player.init(glm::vec3(11.5f, 3.0f, -16.0f), 90.0f, 0.0f);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // =====================================================================
        // CONFIGURAZIONE NPC (Logica e Grafica)
        // =====================================================================

        // --- 1. SETUP GRAFICO: Registra i modelli 3D nel manager delle animazioni ---
        npcAnimManager.init({
           { "door_guard_r", "assets/models/guard_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f), { {0, 255, 1.0f, 0} } },
           { "door_guard_l", "assets/models/guard_npc.gltf", "mixamo.com", 0, glm::mat4(1.0f), { {0, 255, 1.0f, 0} } }
        });

        // --- 2. SETUP LOGICO: Estrae i dialoghi e le info dal JSON ---
        std::unordered_map<std::string, TavernNPC> npcDataFromJson;
        try {
            std::ifstream ifs("assets/scenes/scene.json");
            if(ifs.is_open()) {
                nlohmann::json js;
                ifs >> js;
                ifs.close();
                if(js.contains("instances")) {
                    for(const auto &tech : js["instances"]) {
                        if(tech["technique"].template get<std::string>() == "AnimTech") {
                            for(const auto &el : tech["elements"]) {
                                std::string npcId = el["id"].template get<std::string>();
                                TavernNPC data;
                                data.name = npcId;

                                data.prompt = el.value("prompt", "Premi E per interagire");
                                data.interactionRadius = el.value("interactionRadius", 10.0f);

                                if(el.contains("dialogues")) {
                                    for(const auto &d : el["dialogues"]) {
                                        data.dialogues.push_back(d.template get<std::string>());
                                    }
                                }
                                npcDataFromJson[npcId] = data;
                            }
                        }
                    }
                }
            }
        } catch(...) {
            std::cout << "Warning: could not parse scene.json for NPC dialogues\n";
        }

        // --- 3. SETUP LOGICO: Popola tavernNPCs dalle istanze della scena ---
        tavernNPCs.clear();
        if(SC.TI != nullptr && SC.TechniqueInstanceCount > 1 && SC.TI[1].I != nullptr) {
            for(int i = 0; i < SC.TI[1].InstanceCount; ++i) {
                const auto &inst = SC.TI[1].I[i];
                TavernNPC npc;
                npc.name = *(inst.id);
                npc.position = glm::vec3(inst.Wm[3][0], inst.Wm[3][1], inst.Wm[3][2]);

                auto it = npcDataFromJson.find(npc.name);
                if(it != npcDataFromJson.end()) {
                    npc.prompt = it->second.prompt;
                    npc.interactionRadius = it->second.interactionRadius;
                    npc.dialogues = it->second.dialogues;
                } else {
                    npc.prompt = "Premi E per interagire";
                    npc.interactionRadius = 10.0f;
                    npc.dialogues.push_back("Benvenuto nella taverna.");
                }
                tavernNPCs.push_back(npc);
            }
        }

        if(tavernNPCs.empty()) {
            tavernNPCs = {
                {"door_guard_r", glm::vec3(9.0f, 0.0f, -8.0f), 10.0f, "Premi E per interagire", {"Benvenuto nella taverna.", "Puoi riposare qui."}},
                {"door_guard_l", glm::vec3(13.8f, 0.0f, -8.0f), 10.0f, "Premi E per interagire", {"La porta è chiusa per stanotte.", "Non disturbare i clienti."}}
            };
        }

        // NON TOGLIERE: Trucco anti-crash per il buffer vuoto del TextMaker
        txt.print(-100.0f, -100.0f, " ");

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsInit() override {
        RP.create();
        P.create(&RP);
        Panim.create(&RP);

        DSglobal.init(this, &DSLglobal, {});

        SC.pipelinesAndDescriptorSetsInit();
        txt.pipelinesAndDescriptorSetsInit();
    }

    void pipelinesAndDescriptorSetsCleanup() override {
        P.cleanup();
        Panim.cleanup();
        RP.cleanup();

        DSglobal.cleanup();

        SC.pipelinesAndDescriptorSetsCleanup();
        txt.pipelinesAndDescriptorSetsCleanup();
    }

    void localCleanup() override {
        DSLlocal.cleanup();
        DSLglobal.cleanup();

        P.destroy();
        Panim.destroy();

        npcAnimManager.cleanup();
        dialogueManager.cleanup(txt);

        RP.destroy();

        SC.localCleanup();
        txt.localCleanup();
    }

    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
        auto *T = static_cast<DungeonTavern *>(Params);
        T->populateCommandBuffer(commandBuffer, currentImage);
    }

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
        RP.begin(commandBuffer, currentImage);
        SC.populateCommandBuffer(commandBuffer, 0, currentImage);
        RP.end(commandBuffer);
    }

    void updateUniformBuffer(uint32_t currentImage) override {
       static double lastTime = glfwGetTime();
       double now = glfwGetTime();
       float deltaT = static_cast<float>(now - lastTime);
       lastTime = now;

       if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
          glfwSetWindowShouldClose(window, GL_TRUE);
       }

       // =========================================================
       // 1. UPDATE GAME LOGIC (Dialoghi, Input, Collisioni)
       // =========================================================

       dialogueManager.update(window, deltaT, player, tavernNPCs, txt, windowWidth);

       // Il giocatore può muoversi solo se NON sta parlando
       if(!dialogueManager.isDialogueActive()) {
           player.processInput(window, deltaT);
       }

       // =========================================================
       // 2. UPDATE GRAPHICS (Matrici, Uniforms, Rendering)
       // =========================================================

       // Matrice ViewProjection calcolata dal Player
       glm::mat4 ViewPrj = player.getViewProjectionMatrix(Ar);

       static float lightRotationAngle = 0.0f;
       lightRotationAngle += -0.5f * deltaT;

       const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(lightRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
       const glm::vec3 lightDir =  glm::vec3(lightView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

       GlobalUniformBufferObject gubo{};
       gubo.lightDir = lightDir;
       gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)*5.0f;
       gubo.eyePos = player.position;

       DSglobal.map((int)currentImage, &gubo, 0);

       UniformBufferObject ubo{};
       for(int i = 0; i < SC.TI[0].InstanceCount; i++) {
          ubo.mMat = SC.TI[0].I[i].Wm;
          ubo.mvpMat = ViewPrj * ubo.mMat;

          SC.TI[0].I[i].DS[0][0]->map((int)currentImage, &gubo, 0);
          SC.TI[0].I[i].DS[0][1]->map((int)currentImage, &ubo, 0);
       }

       // Aggiornamento finale dei modelli animati
       npcAnimManager.update(SC, currentImage, gubo, ViewPrj, deltaT);

       // Aggiornamento FPS
       static float elapsedT = 0.0f;
       static int countedFrames = 0;

       countedFrames++;
       elapsedT += deltaT;
       if(elapsedT > 1.0f) {
          float Fps = (float)countedFrames / elapsedT;
          elapsedT = 0.0f;
          countedFrames = 0;
       }

       txt.updateCommandBuffer();
    }
};

// Main Entry Point
int main() {
    DungeonTavern app;

    try {
        app.run(false);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}