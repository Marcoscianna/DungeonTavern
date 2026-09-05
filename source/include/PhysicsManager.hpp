#ifndef PHYSICS_MANAGER_HPP
#define PHYSICS_MANAGER_HPP

#include <vector>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Scene;
class Player;
class Collider;
class TextMaker;
struct GLFWwindow;

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
    bool xPressedLastFrame = false;
    bool lbPressedLastFrame = false;

    std::vector<PhysicsObject> physicsObjects;

    // Lista dei collider customizzati letti dal JSON
    std::vector<Collider*> customColliders;

    Collider* floorCollider;
    float gravity;

    int heldObjectIndex;
    bool qPressedLastFrame;
    bool tPressedLastFrame;
    PhysicsUIState uiState;

public:
    PhysicsManager();
    ~PhysicsManager();

    void init( Scene& scene, const std::string& sceneFilePath = "assets/scenes/scene.json", float floorLevel = 0.0f);
    void update(GLFWwindow* window, float deltaT, Scene& scene, const Player& player, bool canInteract, TextMaker& txt);

    Collider* getFloorCollider() const;
    const std::vector<Collider*>& getCustomColliders() const;

    int getHeldInstanceIndex() const;

    void throwObject(Scene& scene, const std::string& instanceName, glm::vec3 startPos, glm::vec3 velocity);

    // Esponi la dimensione della cella
    float getCellSize() const { return 5.0f; }

    // Esponi la griglia spaziale popolata nell'ultimo frame
    const std::unordered_map<int, std::vector<Instance*>>& getSpatialGrid() const {
        return spatialGrid;
    }

    // Esponi la logica di calcolo dell'ID
    static int getCellID(glm::vec3 pos, float cellSize) {
        int x = static_cast<int>(std::floor(pos.x / cellSize));
        int y = static_cast<int>(std::floor(pos.y / cellSize));
        int z = static_cast<int>(std::floor(pos.z / cellSize));
        return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
    }

private:
    std::unordered_map<int, std::vector<Instance*>> spatialGrid;
};

#endif // PHYSICS_MANAGER_HPP