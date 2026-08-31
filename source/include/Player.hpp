#ifndef PLAYER_HPP
#define PLAYER_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

// Forward declarations per evitare inclusione circolare
class Scene;
class PhysicsManager;
class Collider;

class Player {
public:
    // Posizione e orientamento
    glm::vec3 position;
    float yaw;
    float pitch;

    // Parametri di movimento
    float moveSpeed;
    float rotSpeed;
    bool flyMode;
    bool mPressedLastFrame;
    float playerVelocityY;

    // Parametri di proiezione
    float FOVy;
    float nearPlane;
    float farPlane;

    // Gestione input mouse
    double lastMouseX;
    double lastMouseY;
    bool mouseLookInitialized;

    // Collider dinamico
    Collider* playerCollider;
    float colliderRadius;

    // --- TELECAMERE FISSE (CCTV TAVERNA) ---
    // Mode 0: Prima Persona
    // Mode 1: Telecamera Angolo 1
    // Mode 2: Telecamera Angolo 2
    int cameraMode = 0;
    bool cPressedLastFrame = false;

    // In Player.hpp dentro la sezione private / protected:
    bool isThirdPerson = false;
    bool xPressedLastFrame = false;

    // Telecamera 1
    glm::vec3 fixedCamPos1 = glm::vec3(31.0f, 10.8f, 27.0f);
    glm::vec3 fixedCamTarget1 = glm::vec3(12.0f, 6.0f, 12.0f);

    // Telecamera 2
    glm::vec3 fixedCamPos2 = glm::vec3(1.50103f, 10.877f, 35.689f);
    glm::vec3 fixedCamTarget2 = glm::vec3(11.8589f, 4.97034f, 11.5138f);

    // Costruttori e distruttore
    Player(glm::vec3 startPos = glm::vec3(0.0f, 2.0f, 0.0f));
    ~Player();

    void init(glm::vec3 startPos = glm::vec3(0.0f, 2.0f, 0.0f), float startYaw = -90.0f, float startPitch = 0.0f, float radius = 0.3f);

    // Vettori di direzione
    glm::vec3 getForwardVector() const;
    glm::vec3 getRightVector() const;
    glm::vec3 getUpVector() const;

    // Helper per verificare se siamo in una telecamera fissa
    bool isFixedCamera() const { return cameraMode != 0; }

    bool getisThirdPerson() const { return isThirdPerson; }

    // Aggiornamento dello stato
    void updateMouseLook(GLFWwindow* window);
    bool checkCollisionAt(const glm::vec3& testPos, const Scene& scene, const PhysicsManager& physManager);
    void processInput(GLFWwindow* window, float deltaTime, const Scene& scene, const PhysicsManager& physManager);

    // Matrici della telecamera
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    glm::mat4 getViewProjectionMatrix(float aspectRatio) const;

    // Matrice World per la skin 3D del player
    glm::mat4 getWorldMatrix() const;
};

#endif // PLAYER_HPP