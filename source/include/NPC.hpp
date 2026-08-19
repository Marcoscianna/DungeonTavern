//
// Created by Marco Scianna on 21/06/26.
//

#ifndef SKELETONTOCHANGE_NPC_H
#define SKELETONTOCHANGE_NPC_H
#include <string>
#include <vector>
#include <json.hpp>

#include "glm/fwd.hpp"
#include "modules/Starter.hpp"
#include "modules/Scene.hpp"
#include "Player.hpp"
#include "modules/Animations.hpp"

struct AnimBlendSegment;
struct GlobalUniformBufferObject;

struct AnimNPCDefinition {
	std::string sceneInstanceId;
	std::string animationAssetPath;
	std::string baseTrackName;
	int skinId = 0;
	glm::mat4 modelAdjustment = glm::mat4(1.0f);
	std::vector<AnimBlendSegment> blendSegments;
};

struct AnimUniformBufferObject {
	alignas(16) glm::mat4 mvpMat;
	alignas(16) glm::mat4 mMat;
	alignas(16) glm::mat4 bones[128];
};

// Struttura LOGICA degli NPC (usata per interazioni, dialoghi e distanze)
struct TavernNPC {
	std::string name;
	glm::vec3 position;
	float interactionRadius;
	std::string prompt;
	std::vector<std::string> dialogues;
};

class AnimNPC {
	public:
	std::string sceneInstanceId;
	glm::mat4 modelAdjustment = glm::mat4(1.0f);
	AssetFile assetFile;
	Animations animations;
	SkeletalAnimation skeleton;
	AnimBlender blender;
	bool initialized = false;

	void init(const AnimNPCDefinition &def) {
		sceneInstanceId = def.sceneInstanceId;
		modelAdjustment = def.modelAdjustment;

		assetFile.init(def.animationAssetPath, GLTF);
		animations.init(assetFile);
		skeleton.init(&animations, 1, def.baseTrackName, def.skinId);
		blender.init(def.blendSegments);
		initialized = true;
	}

	void play(int segment, float blendTime = 0.0f) {
		if(!initialized) {
			throw std::runtime_error("Animated NPC not initialized");
		}

		blender.Start(segment, blendTime);
	}

	void updateAndUpload(Scene &scene, uint32_t currentImage, GlobalUniformBufferObject &gubo, const glm::mat4 &viewProj) {
		if(!initialized) {
			throw std::runtime_error("Animated NPC not initialized");
		}

		auto instanceIt = scene.InstanceIds.find(sceneInstanceId);
		if(instanceIt == scene.InstanceIds.end()) {
			throw std::runtime_error("Animated NPC instance not found in scene: " + sceneInstanceId);
		}

		int instanceIndex = instanceIt->second;
		Instance *instance = scene.I[instanceIndex];

		AnimUniformBufferObject aubo{};
		std::vector<glm::mat4> *boneMatrices = skeleton.getTransformMatrices();
		int boneCount = skeleton.getNTMs();

		for(int b = 0; b < 128; ++b) {
			if(b < boneCount) {
				aubo.bones[b] = (*boneMatrices)[b];
			} else {
				aubo.bones[b] = glm::mat4(1.0f);
			}
		}

		aubo.mMat = instance->Wm * modelAdjustment;
		aubo.mvpMat = viewProj * aubo.mMat;

		instance->DS[0][0]->map((int)currentImage, &gubo, 0);
		instance->DS[0][1]->map((int)currentImage, &aubo, 0);
	}

	void advance(float deltaT) {
		if(!initialized) {
			throw std::runtime_error("Animated NPC not initialized");
		}

		blender.Advance(deltaT);
		skeleton.Sample(blender);
	}

	void cleanup() {
		skeleton.cleanup();
		animations.cleanup();
		assetFile.cleanup();
	}
};

class AnimatedNPCRig {
	public:
	std::vector<AnimNPC> NPCs;

	void init(const std::vector<AnimNPCDefinition> &defs) {
		NPCs.clear();
		NPCs.reserve(defs.size());

		for(const auto &def : defs) {
			NPCs.emplace_back();
			NPCs.back().init(def);
		}
	}

	AnimNPC *find(const std::string &sceneInstanceId) {
		for(auto &npc : NPCs) {
			if(npc.sceneInstanceId == sceneInstanceId) {
				return &npc;
			}
		}
		return nullptr;
	}

	void play(const std::string &sceneInstanceId, int segment, float blendTime = 0.0f) {
		AnimNPC *npc = find(sceneInstanceId);
		if(npc == nullptr) {
			throw std::runtime_error("Animated NPC not found: " + sceneInstanceId);
		}

		npc->play(segment, blendTime);
	}

	void update(Scene &scene, uint32_t currentImage, GlobalUniformBufferObject &gubo, const glm::mat4 &viewProj, float deltaT) {
		for(auto &npc : NPCs) {
			npc.advance(deltaT);
			npc.updateAndUpload(scene, currentImage, gubo, viewProj);
		}
	}

	void cleanup() {
		for(auto &npc : NPCs) {
			npc.cleanup();
		}
		NPCs.clear();
	}
};

#endif //SKELETONTOCHANGE_NPC_H
