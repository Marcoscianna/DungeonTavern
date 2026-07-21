#ifndef PLAYER_HPP
#define PLAYER_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class Player {
public:
    // Posizione e orientamento
    glm::vec3 position;
    float yaw;   // Rotazione orizzontale in gradi
    float pitch; // Rotazione verticale in gradi

    // Parametri di movimento e vista
    float moveSpeed;
    float rotSpeed; // Sensibilità mouse
    float FOVy;
    float nearPlane;
    float farPlane;

    // Stato del mouse
    double lastMouseX;
    double lastMouseY;
    bool mouseLookInitialized;

    // Costruttore
    Player(glm::vec3 startPos = glm::vec3(0.0f, 1.2f, 3.0f));

    // Inizializzazione posizione/orientamento
    void init(glm::vec3 startPos = glm::vec3(0.0f, 1.2f, 3.0f), float startYaw = -90.0f, float startPitch = 0.0f);

    // Vettori di direzione
    glm::vec3 getForwardVector() const;
    glm::vec3 getRightVector() const;
    glm::vec3 getUpVector() const;

    // Gestione dell'input per movimento WASD/Spazio/Shift e rotazione mouse
    void processInput(GLFWwindow* window, float deltaTime);
    void updateMouseLook(GLFWwindow* window);

    // Matrici per Vulkan
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    glm::mat4 getViewProjectionMatrix(float aspectRatio) const;
};

#endif // PLAYER_HPP