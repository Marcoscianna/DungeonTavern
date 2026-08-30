#ifndef PLAYER_HPP
#define PLAYER_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

// Forward declarations per evitare problemi di inclusione
class Scene;
class Collider;
class PhysicsManager;

class Player {
    Collider* playerCollider; // Usiamo un puntatore al collider

public:
    // Posizione e orientamento
    glm::vec3 position;
    float yaw;   // Rotazione orizzontale in gradi
    float pitch; // Rotazione verticale in gradi

    // Parametri di movimento e vista
    float moveSpeed;
    float rotSpeed; // Sensibilità mouse
    float playerVelocityY;
    float FOVy;
    float nearPlane;
    float farPlane;

    float colliderRadius;

    // Stato del mouse
    double lastMouseX;
    double lastMouseY;
    bool mouseLookInitialized;

    //Debug
    bool flyMode;
    bool mPressedLastFrame;

    // Costruttore e Distruttore
    Player(glm::vec3 startPos = glm::vec3(0.0f, 1.2f, 3.0f));
    ~Player();

    // Inizializzazione posizione/orientamento e collider
    void init(glm::vec3 startPos = glm::vec3(0.0f, 1.2f, 3.0f), float startYaw = -90.0f, float startPitch = 0.0f, float radius = 0.5f);

    // Vettori di direzione
    glm::vec3 getForwardVector() const;
    glm::vec3 getRightVector() const;
    glm::vec3 getUpVector() const;

    // Gestione dell'input per movimento WASD/Spazio/Shift e risoluzione collisioni
    void processInput(GLFWwindow* window, float deltaTime, const Scene& scene, const PhysicsManager& physManager);
    bool checkCollisionAt(const glm::vec3& testPos, const Scene& scene, const PhysicsManager& physManager);
    void updateMouseLook(GLFWwindow* window);

    // Helper per verificare collisioni a una data coordinata
    bool checkCollisionAt(const glm::vec3& testPos, const Scene& scene);

    // Matrici per Vulkan
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    glm::mat4 getViewProjectionMatrix(float aspectRatio) const;
    Collider* getCollider() const { return playerCollider; }
    glm::mat4 getWorldMatrix() const;
};

#endif // PLAYER_HPP