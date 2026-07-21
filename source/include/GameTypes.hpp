#pragma once
#include <string>
#include <glm/glm.hpp>

enum class AppMode {
    Exploration,
    Dialogue
};

struct NPCData {
    std::string name;
    glm::vec3 position;
    float interactionRadius;
    std::string dialogue;
};