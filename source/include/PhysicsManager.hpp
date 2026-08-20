#ifndef PHYSICS_MANAGER_HPP
#define PHYSICS_MANAGER_HPP

#include <vector>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// Forward declarations
struct GLFWwindow;
class Scene;
class Player;
class Collider;
class TextMaker;

struct PhysicsObject {
    int instanceIndex;
    glm::vec3 velocity;
    glm::vec3 angularVelocity; // Per la rotazione in volo
    float mass;                // Resistenza alla spinta
    float bounciness;          // Coefficiente di restituzione (rimbalzo)
    float friction;            // Attrito col terreno
    bool isHeld;
    glm::vec3 originalScale;
    glm::quat originalRotation;
};

enum class PhysicsUIState { NONE, GRAB, HOLD };

class PhysicsManager {
private:
    std::vector<PhysicsObject> physicsObjects;
    Collider* floorCollider;
    float gravity;

    int heldObjectIndex;
    bool ePressedLastFrame;
    bool tPressedLastFrame;
    PhysicsUIState uiState;

public:
    PhysicsManager();
    ~PhysicsManager();

    void init(const Scene& scene, const std::string& sceneFilePath = "assets/scenes/scene.json", float floorLevel = 0.0f);
    void update(GLFWwindow* window, float deltaT, Scene& scene, const Player& player, bool canInteract, TextMaker& txt);
    Collider* getFloorCollider();
};

#endif // PHYSICS_MANAGER_HPP