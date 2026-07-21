// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>
#include <vector>
#include <cmath>

#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "modules/Animations.hpp"

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
	alignas(16) glm::vec4 jointIndices;
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
	float Ar;	// Aspect ratio

	glm::mat4 ViewPrj;
	glm::mat4 View;

    glm::vec3 camPos;
    float yaw;
    float pitch, moveSpeed, rotSpeed;
    double lastMouseX;
    double lastMouseY;
    bool mouseLookInitialized;
    bool showInteractionPrompt;
    int activeNPC;
    std::vector<TavernNPC> tavernNPCs;
	Animations npcAnims;
	SkeletalAnimation guardSkin;
	AnimBlender guardBlender;

	public:
	DungeonTavern()
		: Ar(4.0f / 3.0f), ViewPrj(1.0f), View(1.0f), camPos(0.0f, 1.4f, 5.0f), yaw(-90.0f),
		  pitch(0.0f), moveSpeed(10.0f), rotSpeed(10.0f), lastMouseX(0.0), lastMouseY(0.0),
		  mouseLookInitialized(false), showInteractionPrompt(false), activeNPC(-1) {}

	// Here you set the main application parameters
	void setWindowParameters() override {
		// window size, titile and initial background
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
	
	// Here you load and setup all your Vulkan Models and Texutures.
	// Here you also create your Descriptor set layouts and load the shaders for the pipelines
	void localInit() override {

		// 1. Crea il Layout per l'UBO Animato (identico al locale, ma con AnimUniformBufferObject)
		DSLanim.init(this, {
				{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(AnimUniformBufferObject), 1},
				{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
		});

		// Descriptor Layouts [what will be passed to the shaders]
		DSLlocal.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObject), 1},
					{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
				  });
		DSLglobal.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
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
							{0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexAnim, jointIndices), sizeof(glm::vec4), JOINTINDEX}
		});

		Panim.init(this, &VDanim,
				   "shaders/skinning.vert.spv",
				   "shaders/toChangeBlinnFromPos.frag.spv",
				   {&DSLglobal, &DSLanim});

		// initializes the render passes
		RP.init(this);
		// sets the blue sky
		RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};

		// Pipelines [Shader couples]
		// The last array, is a vector of pointer to the layouts of the sets that will
		// be used in this pipeline. The first element will be set 0, and so on..
		
		P.init(this, &VD, "shaders/toChangeSimplePos.vert.spv",
						  "shaders/toChangeBlinnFromPos.frag.spv",
						  {&DSLglobal, &DSLlocal});


		// sets the size of the Descriptor Set Pool (it MUST be done before loading the scene)
		DPSZs.uniformBlocksInPool = 20;
		DPSZs.texturesInPool = 10;
		DPSZs.setsInPool = 20;

		// to support scene
		VDRs.resize(2);
		VDRs[0].init("VDposUV",  &VD);
		VDRs[1].init("VDanim",   &VDanim);

		PRs.resize(2);
		PRs[0].init("BlinnPos", {
							{&P, {//Pipeline and DSL for the main pass
							 /*DSLglobal*/{},
							 /*DSLlocal*/{
									/*t0*/{true,  0, {}}
								  }
								 }
								}
						  }, /*TotalNtextures*/1, &VD);


		// Tecnica 1: Animata
		PRs[1].init("AnimTech", {
				{&Panim, { // Usa la pipeline animata
						/*DSLglobal*/{},
						/*DSLanim*/{
											 /*t0*/{true,  0, {}} // Una texture per ora (la Diffuse)
									 }
				}
				}
		}, /*TotalNtextures*/1, &VDanim); // Usa VDanim

		if(SC.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0) {
			std::cout << "ERROR LOADING THE SCENE\n";
			exit(0);
		}

		// initializes the textual output
		txt.init(this, (int)windowWidth, (int)windowHeight);

		camPos = glm::vec3(0.0f, 1.4f, 5.0f);
		yaw = -90.0f;
		pitch = 0.0f;
		moveSpeed = 10.0f;
		rotSpeed = 10.0f;
		lastMouseX = 0.0;
		lastMouseY = 0.0;
		mouseLookInitialized = false;
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
		txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});

	}
	
	// Here you create your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsInit() override {
		// creates the render passes
		RP.create();
		
		// This creates a new pipeline (with the current surface), using its shaders for the provided render pass
		P.create(&RP);
		Panim.create(&RP);
		
		DSglobal.init(this, &DSLglobal, {});
		
		// Here you define the data set
		// If the scene has textures coming from a render pass, the corresponding element of the technique must be
		// updated before calling SC.pipelinesAndDescriptorSetsInit();

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
	// You also have to destroy the pipelines
	void localCleanup() override {
		DSLlocal.cleanup();
		DSLglobal.cleanup();

		P.destroy();
		Panim.destroy();

		RP.destroy();

		SC.localCleanup();
		txt.localCleanup();
	}
	
	// Here it is the creation of the command buffer:
	// You send to the GPU all the objects you want to draw,
	// with their buffers and textures
	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
		// Simple trick to avoid having always 'T->'
		// in che code that populates the command buffer!
		auto *T = static_cast<DungeonTavern *>(Params);
		T->populateCommandBuffer(commandBuffer, currentImage);
	}

	void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
		
		// Offscreen pass - always required
		// begin standard pass
		RP.begin(commandBuffer, currentImage);

		SC.populateCommandBuffer(commandBuffer, 0, currentImage);

		RP.end(commandBuffer);
	}

	// Here is where you update the uniforms.
	// Very likely this will be where you will be writing the logic of your application.
	void updateUniformBuffer(uint32_t currentImage) override {
		static bool debounce = false;
		static int curDebounce = 0;
		float deltaT;
		glm::vec3 m(0.0f);
		glm::vec3 r(0.0f);
		bool fire = false;

		// handle the ESC key to exit the app
		if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// moves the view
		getSixAxis(deltaT, m, r, fire);
		updateMouseLook();
		const float deltaYaw = glm::radians(rotSpeed) * deltaT;
		const float deltaPitch = glm::radians(rotSpeed) * deltaT;
		if(glfwGetKey(window, GLFW_KEY_LEFT)) yaw -= glm::degrees(deltaYaw);
		if(glfwGetKey(window, GLFW_KEY_RIGHT)) yaw += glm::degrees(deltaYaw);
		if(glfwGetKey(window, GLFW_KEY_UP)) pitch += glm::degrees(deltaPitch);
		if(glfwGetKey(window, GLFW_KEY_DOWN)) pitch -= glm::degrees(deltaPitch);
		pitch = glm::clamp(pitch, -89.0f, 89.0f);

		const glm::vec3 forward = glm::normalize(glm::vec3(
			cos(glm::radians(yaw)),
			0.0f,
			sin(glm::radians(yaw))
		));
		const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::vec3 move(0.0f);
		if(glfwGetKey(window, GLFW_KEY_W)) move += forward;
		if(glfwGetKey(window, GLFW_KEY_S)) move -= forward;
		if(glfwGetKey(window, GLFW_KEY_A)) move -= right;
		if(glfwGetKey(window, GLFW_KEY_D)) move += right;
		if(glm::length(move) > 0.0f) {
			camPos += glm::normalize(move) * moveSpeed * deltaT;
		}
		camPos.y = 1.4f;

		glm::vec3 interactionTarget(0.0f);
		showInteractionPrompt = false;
		activeNPC = -1;
		float bestDistance = 99999.0f;
		for(size_t i = 0; i < tavernNPCs.size(); ++i) {
			float d = glm::length(camPos - tavernNPCs[i].position);
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
			std::ostringstream oss;
			oss << "Talking to " << tavernNPCs[activeNPC].name << "...";
			txt.print(1.0f, 28.0f, oss.str(), 1, "CO", false, false, true, TAL_LEFT, TRH_LEFT, TRV_BOTTOM, {1.0f,0.9f,0.7f,1.0f}, {0.15f,0.05f,0.0f,0.85f});
		}
		if(!glfwGetKey(window, GLFW_KEY_E)) {
			debounce = false;
		}
		if(curDebounce > 0) {
			--curDebounce;
		}

		updateViewProjection();

		// defines the global parameters for the uniform
		static float lightRotationAngle = 0.0f; // Static variable to keep track of rotation
		lightRotationAngle += -0.5f * deltaT; // Increment rotation angle based on time

		const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(lightRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f)) * 
										glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::vec3 lightDir =  glm::vec3(lightView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

		GlobalUniformBufferObject gubo{};

		gubo.lightDir = lightDir;
		gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)*5.0f;
		gubo.eyePos = camPos;

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

		for(int b = 0; b < 128; b++) {
			aubo.bones[b] = glm::mat4(1.0f);
		}

		for(int i = 0; i < SC.TI[1].InstanceCount; i++) {
			aubo.mMat = SC.TI[1].I[i].Wm;
			aubo.mvpMat = ViewPrj * aubo.mMat;

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
			std::ostringstream oss;
			oss << "FPS: " << Fps << "\n";
			if(showInteractionPrompt && activeNPC >= 0) {
				oss << tavernNPCs[activeNPC].prompt << "\n";
			}

			txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});
			
			elapsedT = 0.0f;
		    countedFrames = 0;
		}
		
		txt.updateCommandBuffer();
	}
	
	glm::vec3 getForwardVector() const {
		return glm::normalize(glm::vec3(
			cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
			sin(glm::radians(pitch)),
			sin(glm::radians(yaw)) * cos(glm::radians(pitch))
		));
	}

	void updateMouseLook() {
		double xpos = 0.0;
		double ypos = 0.0;
		glfwGetCursorPos(window, &xpos, &ypos);
		if(!mouseLookInitialized) {
			lastMouseX = xpos;
			lastMouseY = ypos;
			mouseLookInitialized = true;
			return;
		}

		const float mouseSensitivity = 0.08f;
		const float dx = static_cast<float>(xpos - lastMouseX);
		const float dy = static_cast<float>(ypos - lastMouseY);
		lastMouseX = xpos;
		lastMouseY = ypos;

		yaw += dx * mouseSensitivity;
		pitch -= dy * mouseSensitivity;
		pitch = glm::clamp(pitch, -89.0f, 89.0f);
	}

	void updateViewProjection() {
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f);
		const float nearPlane = 0.1f;
		const float farPlane = 100.f;

		// Projection
		glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
		Prj[1][1] *= -1;

		// View
		const glm::vec3 forward = getForwardVector();
		View = glm::lookAt(camPos, camPos + forward, glm::vec3(0.0f, 1.0f, 0.0f));

		// View-Projection
		ViewPrj = Prj * View;
	}
};


// This is the main: probably you do not need to touch this!
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
