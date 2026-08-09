// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>
#include <vector>
#include <cmath>

#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/Animations.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "Player.hpp" // Include della classe Player

// The uniform buffer object used in this example
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

struct TavernNPC {
    std::string name;
    glm::vec3 position;
    float interactionRadius;
    std::string prompt;
};

// L'UBO per i modelli animati (contiene l'array delle ossa)
struct AnimUniformBufferObject {
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 bones[128]; // Array per lo scheletro (max 128 ossa)
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
    // Here you list all the Vulkan objects you need:

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
    std::vector<VertexDescriptorRef>  VDRs;
    std::vector<TechniqueRef> PRs;

    // to provide textual feedback
    TextMaker txt;

    // Other application parameters
    float Ar;  // Aspect ratio

    // Oggetto Player per gestire movimento e visuale
    Player player;

    bool showInteractionPrompt;
    int activeNPC;
    std::vector<TavernNPC> tavernNPCs;
    Animations npcAnims;
    SkeletalAnimation guardSkin;
    AnimBlender guardBlender;

    public:
    DungeonTavern()
       : Ar(4.0f / 3.0f),
         showInteractionPrompt(false), activeNPC(-1) {}

    // Here you set the main application parameters
    void setWindowParameters() override {
       // window size, title and initial background
       windowWidth = 800;
       windowHeight = 600;
       windowTitle = "Dungeon Tavern";
       windowResizable = GLFW_TRUE;

       // Initial aspect ratio
       Ar = 4.0f / 3.0f;
    }

    // What to do when the window changes size
    void onWindowResize(int w, int h) override {
       std::cout << "Window resized to: " << w << " x " << h << "\n";
       Ar = (float)w / (float)h;
       // Update Render Pass
       RP.width = w;
       RP.height = h;

       // updates the textual output
       txt.resizeScreen((int)w, (int)h);
    }

    // Here you load and setup all your Vulkan Models and Textures.
    // Here you also create your Descriptor set layouts and load the shaders for the pipelines
    void localInit() override {

       // 1. Crea il Layout per l'UBO Animato
       DSLanim.init(this, {
             {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(AnimUniformBufferObject), 1},
             {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
       });

       // Descriptor Layouts [what will be passed to the shaders]
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
               {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos),
                      sizeof(glm::vec3), POSITION},
               {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV),
                      sizeof(glm::vec2), UV}
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

       // initializes the render passes
       RP.init(this);
       // sets the blue sky
       RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};

       P.init(this, &VD, "shaders/toChangeSimplePos.vert.spv",
                     "shaders/toChangeBlinnFromPos.frag.spv",
                     {&DSLglobal, &DSLlocal});

       // sets the size of the Descriptor Set Pool
       DPSZs.uniformBlocksInPool = 20;
       DPSZs.texturesInPool = 10;
       DPSZs.setsInPool = 20;

       // to support scene
       VDRs.resize(2);
       VDRs[0].init("VDposUV",  &VD);
       VDRs[1].init("VDanim",   &VDanim);

       PRs.resize(2);
       PRs[0].init("BlinnPos", {
                      {&P, {
                       /*DSLglobal*/{},
                       /*DSLlocal*/{
                            /*t0*/{true,  0, {}}
                           }
                          }
                         }
                     }, /*TotalNtextures*/1, &VD);

       // Tecnica 1: Animata
       PRs[1].init("AnimTech", {
             {&Panim, {
                   /*DSLglobal*/{},
                   /*DSLanim*/{
                                   /*t0*/{true,  0, {}}
                             }
             }
             }
       }, /*TotalNtextures*/1, &VDanim);

       if(SC.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0) {
          std::cout << "ERROR LOADING THE SCENE\n";
          exit(0);
       }

       // --- Inizializza animazioni per guard_npc (carica separatamente l'asset GLTF)
       {
           AssetFile *guardAF = new AssetFile();
           guardAF->init("assets/models/guard_npc.gltf", GLTF);
           npcAnims.init(*guardAF);
           guardSkin.init(&npcAnims, 1, "mixamo.com", 0);
           AnimBlendSegment seg = { 0, 255, 1.0f, 0 };
           guardBlender.init(std::vector<AnimBlendSegment>{seg});
       }

       // initializes the textual output
       txt.init(this, (int)windowWidth, (int)windowHeight);

       // Inizializzazione del Player
       player.init(glm::vec3(0.0f, 1.4f, 5.0f), -90.0f, 0.0f);

       showInteractionPrompt = false;
       activeNPC = -1;
       tavernNPCs = {
          {"Innkeeper", glm::vec3(0.0f, 0.0f, 0.0f), 1.75f, "Press E to talk to the Innkeeper"},
          {"Bard", glm::vec3(2.5f, 0.0f, -2.0f), 1.75f, "Press E to listen to the Bard"},
          {"Merchant", glm::vec3(-2.5f, 0.0f, -1.5f), 1.75f, "Press E to trade with the Merchant"}
       };
       glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

       // submits the main command buffer
       submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

       // Prepares for showing the FPS count
    }

    // Here you create your pipelines and Descriptor Sets!
    void pipelinesAndDescriptorSetsInit() override {
       RP.create();
       P.create(&RP);
       Panim.create(&RP);

       DSglobal.init(this, &DSLglobal, {});

       SC.pipelinesAndDescriptorSetsInit();
       txt.pipelinesAndDescriptorSetsInit();
    }

    // Here you destroy your pipelines and Descriptor Sets!
    void pipelinesAndDescriptorSetsCleanup() override {
       P.cleanup();
       Panim.cleanup();
       RP.cleanup();

       DSglobal.cleanup();

       SC.pipelinesAndDescriptorSetsCleanup();
       txt.pipelinesAndDescriptorSetsCleanup();
    }

    // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
    void localCleanup() override {
       DSLlocal.cleanup();
       DSLglobal.cleanup();

       P.destroy();
       Panim.destroy();

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

    // Here is where you update the uniforms.
    void updateUniformBuffer(uint32_t currentImage) override {
       static bool debounce = false;
       static int curDebounce = 0;
       static double lastTime = glfwGetTime();
       double now = glfwGetTime();
       float deltaT = static_cast<float>(now - lastTime);
       lastTime = now;
       // handle the ESC key to exit the app
       if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
          glfwSetWindowShouldClose(window, GL_TRUE);
       }

       // Gestione dell'input (movimento WASD e orientamento mouse) tramite Player
       player.processInput(window, deltaT);

       // Forza l'altezza della testa/camera fissa
       //player.position.y = 1.4f;

       // Calcolo distanza interazione NPC basato su player.position
       glm::vec3 interactionTarget(0.0f);
       showInteractionPrompt = false;
       activeNPC = -1;
       float bestDistance = 99999.0f;
       for(size_t i = 0; i < tavernNPCs.size(); ++i) {
          float d = glm::length(player.position - tavernNPCs[i].position);
          if(d < tavernNPCs[i].interactionRadius && d < bestDistance) {
             bestDistance = d;
             activeNPC = static_cast<int>(i);
             showInteractionPrompt = true;
             interactionTarget = tavernNPCs[i].position;
          }
       }

       if(showInteractionPrompt && glfwGetKey(window, GLFW_KEY_E) && !debounce) {
          debounce = true;
          curDebounce = 12;
       }
       if(!glfwGetKey(window, GLFW_KEY_E)) {
          debounce = false;
       }
       if(curDebounce > 0) {
          --curDebounce;
       }

       // Matrice ViewProjection calcolata dal Player
       glm::mat4 ViewPrj = player.getViewProjectionMatrix(Ar);

       // defines the global parameters for the uniform
       static float lightRotationAngle = 0.0f;
       lightRotationAngle += -0.5f * deltaT;

       const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(lightRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
       const glm::vec3 lightDir =  glm::vec3(lightView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

       GlobalUniformBufferObject gubo{};
       gubo.lightDir = lightDir;
       gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)*5.0f;
       gubo.eyePos = player.position; // Posizione occhio presa da player

       DSglobal.map((int)currentImage, &gubo, 0);

       // defines the local parameters for the uniforms
       UniformBufferObject ubo{};

       for(int i = 0; i < SC.TI[0].InstanceCount; i++) {
          ubo.mMat = SC.TI[0].I[i].Wm;
          ubo.mvpMat = ViewPrj * ubo.mMat;

          SC.TI[0].I[i].DS[0][0]->map((int)currentImage, &gubo, 0);
          SC.TI[0].I[i].DS[0][1]->map((int)currentImage, &ubo, 0);
       }

       AnimUniformBufferObject aubo{};

       // 1. Avanza e campiona le ossa del guard
       guardBlender.Advance(deltaT);
       guardSkin.Sample(guardBlender);

       // 2. Copia le matrici delle ossa nel buffer
       std::vector<glm::mat4> *bm = guardSkin.getTransformMatrices();
       int nB = guardSkin.getNTMs();
       for(int b = 0; b < 128; b++) {
          if(b < nB) {
             aubo.bones[b] = (*bm)[b];
          } else {
             aubo.bones[b] = glm::mat4(1.0f);
          }
       }

       // 3. Mappa i dati sulle istanze animate
       for(int i = 0; i < SC.TI[1].InstanceCount; i++) {
          aubo.mMat = SC.TI[1].I[i].Wm;

          // Scala e rotazione per raddrizzare la guardia
          if(SC.TI[1].I[i].id != nullptr && *SC.TI[1].I[i].id == "guard_1") {
             float s = 0.03f;
             glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(s));
             glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(+90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
             aubo.mMat = aubo.mMat * rotMat * scaleMat;
          }

          aubo.mvpMat = ViewPrj * aubo.mMat;

          // Mappa l'UBO con le ossa aggiornate
          SC.TI[1].I[i].DS[0][0]->map((int)currentImage, &gubo, 0);
          SC.TI[1].I[i].DS[0][1]->map((int)currentImage, &aubo, 0);
       }

       // updates the FPS
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