#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <fstream>
#include <iostream>
#include <GLFW/glfw3.h>
#include <json.hpp>

struct PointLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
};

struct GlobalUniformBufferObject {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 eyePos;
    alignas(16) glm::mat4 lightVP;
    alignas(16) PointLight pLights[50];
    alignas(4)  int numLights;
    float shadowToggle;
};

class LightManager {
private:
    struct InternalLight {
        glm::vec3 position;
        glm::vec3 baseColor;
        float randomOffset;
        float freq1;
        float freq2;
    };

    std::vector<InternalLight> internalLights;

    float timeOfDay = 12.0f;
    float timeSpeed = 0.05f;
    float totalTime = 0.0f;
    bool nPressed = false;

    glm::vec4 currentSkyColor;
    glm::vec4 currentDirLightColor;
    glm::vec3 currentDirLightDir;

public:
    void init(float startTime = 12.0f) {
        timeOfDay = startTime;
        internalLights.clear();
        totalTime = 0.0f;
    }

    void loadLightsFromJson(const std::string& filepath) {
        try {
            std::ifstream ifs(filepath);
            if (ifs.is_open()) {
                nlohmann::json js;
                ifs >> js;
                ifs.close();

                if (js.contains("lights")) {
                    for (const auto& l : js["lights"]) {
                        glm::vec3 pos(l["position"][0], l["position"][1], l["position"][2]);
                        glm::vec3 col(l["color"][0], l["color"][1], l["color"][2]);
                        float intensity = l.value("intensity", 1.0f);

                        // Genera un offset casuale (0.0 -> 10.0) per asincronizzare le fiamme
                        float offset = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f;

                        float f1 = 10.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 25.0f;
                        float f2 = 15.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 20.0f;

                        internalLights.push_back({pos, col * intensity, offset, f1, f2});
                    }
                    std::cout << "LightManager: Caricate " << internalLights.size() << " luci.\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Errore parsing luci nel LightManager: " << e.what() << "\n";
        }
    }

    void update(float deltaT, GLFWwindow* window) {
        totalTime += deltaT;

        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
            if (!nPressed) {
                timeSpeed = (timeSpeed == 0.05f) ? 1.0f : 0.05f;
                nPressed = true;
            }
        } else {
            nPressed = false;
        }

        timeOfDay += timeSpeed * deltaT;
        if (timeOfDay >= 24.0f) timeOfDay -= 24.0f;

        float dayFactor = 0.0f;
        if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
            dayFactor = sin((timeOfDay - 6.0f) / 12.0f * 3.14159265f);
        }

        // 4. Posizione e Colore Luce Direzionale (Sole/Luna)
        float sunAngle = (timeOfDay - 12.0f) / 12.0f * 90.0f;

        glm::mat4 lightView = glm::rotate(glm::mat4(1.0f), glm::radians(sunAngle), glm::vec3(0.0f, 1.0f, 0.0f)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        currentDirLightDir = glm::vec3(lightView * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f));

        glm::vec4 dayLight   = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) * 5.0f;
        glm::vec4 nightLight = glm::vec4(0.15f, 0.25f, 0.6f, 1.0f) * 0.1f;
        currentDirLightColor = glm::mix(nightLight, dayLight, dayFactor);

        glm::vec4 daySky   = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
        glm::vec4 nightSky = glm::vec4(0.01f, 0.01f, 0.02f, 1.0f);
        currentSkyColor = glm::mix(nightSky, daySky, dayFactor);
    }

    void applyToGUBO(GlobalUniformBufferObject& gubo) {
        gubo.lightDir = currentDirLightDir;
        gubo.lightColor = currentDirLightColor;

        gubo.numLights = std::min((int)internalLights.size(), 50);
        for(int i = 0; i < gubo.numLights; i++) {
            gubo.pLights[i].position = internalLights[i].position;

            // --- FLICKERING ---
            float t = totalTime + internalLights[i].randomOffset;

            float wave = sin(t * internalLights[i].freq1) * cos(t * internalLights[i].freq2);
            float flicker = 0.8f + 0.2f * wave;

            gubo.pLights[i].color = internalLights[i].baseColor * flicker;
        }
    }

    float getTimeOfDay() const { return timeOfDay; }

    // Restituisce l'ID dell'istanza dello Skydome da mostrare in base all'ora attuale
    std::string getCurrentSkydomeInstanceId() const {
        if (timeOfDay >= 6.0f && timeOfDay < 17.0f) {
            return "skydome_day";
        } else if (timeOfDay >= 17.0f && timeOfDay < 20.0f) {
            return "skydome_sunset";
        } else if (timeOfDay >= 20.0f && timeOfDay < 23.0f) {
            return "skydome_night";
        } else if (timeOfDay >= 23.0f || timeOfDay < 4.0f) {
            return "skydome_night2";
        } else { // Tra 4.0 e 6.0 (Alba / Prima mattina)
            return "skydome_night3";
        }
    }

    glm::vec4 getSkyColor() { return currentSkyColor; }
    bool isTimeAccelerated() { return timeSpeed > 1.0f; }
};