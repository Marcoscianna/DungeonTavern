#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <json.hpp>

// Struttura dati per singola luce puntiforme (passata tramite UBO al fragment shader)
struct PointLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
};

// Global Uniform Buffer Object: raccoglie i dati globali di illuminazione e ombre per la GPU
struct GlobalUniformBufferObject {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 eyePos;
    alignas(16) glm::mat4 lightVP;
    alignas(16) PointLight pLights[50];
    alignas(4) int numLights;
    alignas(4) float shadowToggle;
    alignas(8) glm::vec2 padding;
};

class LightManager {
private:
    // Struttura interna per gestire parametri avanzati di ogni luce (es. frequenze di flickering personalizzate)
    struct InternalLight {
        glm::vec3 position;
        glm::vec3 baseColor;
        float randomOffset;
        float freq1;
        float freq2;
    };

    std::vector<InternalLight> internalLights;

    float timeOfDay = 12.0f; // Orario corrente della giornata in formato 0.0 - 24.0
    float timeSpeed = 0.05f; // Velocità di scorrimento del tempo
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

    // Caricamento e parsing delle posizioni e colori delle luci dal file JSON della scena
    void loadLightsFromJson(const std::string &filepath) {
        try {
            std::ifstream ifs(filepath);
            if (ifs.is_open()) {
                nlohmann::json js;
                ifs >> js;
                ifs.close();

                if (js.contains("lights")) {
                    internalLights.clear();
                    for (const auto &l: js["lights"]) {
                        glm::vec3 pos(l["position"][0], l["position"][1], l["position"][2]);
                        glm::vec3 col(l["color"][0], l["color"][1], l["color"][2]);
                        float intensity = l.value("intensity", 1.0f);

                        // Generazione di offset e frequenze casuali per asincronizzare
                        // il flickering delle singole torce ed evitare che lampeggino tutte uguali
                        float offset = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 100.0f;
                        float f1 = 8.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 12.0f;
                        float f2 = 18.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 22.0f;

                        internalLights.push_back({pos, col * intensity, offset, f1, f2});
                    }
                    std::cout << "LightManager: Caricate " << internalLights.size() << " luci.\n";
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Errore parsing luci nel LightManager: " << e.what() << "\n";
        }
    }

    void update(float deltaT, GLFWwindow *window) {
        totalTime += deltaT;

        // Tasto 'N': Toggle rapido per accelerare lo scorrimento del tempo (giorno/notte dinamico)
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

        // Calcolo del fattore di luce diurna basato su una funzione seno
        float dayFactor = 0.0f;
        if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
            dayFactor = std::sin((timeOfDay - 6.0f) / 12.0f * 3.14159265f);
        }

        // Calcolo della posizione e matrice di vista della luce direzionale (Sole / Luna)
        float sunAngle = (timeOfDay - 12.0f) / 12.0f * 90.0f;
        glm::mat4 lightView = glm::rotate(glm::mat4(1.0f), glm::radians(sunAngle), glm::vec3(0.0f, 1.0f, 0.0f)) *
                              glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        currentDirLightDir = glm::vec3(lightView * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f));

        // Interpolazione fluida dei colori della luce solare/lunare e del cielo
        glm::vec4 dayLight = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) * 5.0f;
        glm::vec4 nightLight = glm::vec4(0.15f, 0.25f, 0.6f, 1.0f) * 0.1f;
        currentDirLightColor = glm::mix(nightLight, dayLight, dayFactor);

        glm::vec4 daySky = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
        glm::vec4 nightSky = glm::vec4(0.01f, 0.01f, 0.02f, 1.0f);
        currentSkyColor = glm::mix(nightSky, daySky, dayFactor);
    }

    void applyToGUBO(GlobalUniformBufferObject &gubo) {
        gubo.lightDir = currentDirLightDir;
        gubo.lightColor = currentDirLightColor;

        gubo.numLights = std::min(static_cast<int>(internalLights.size()), 50);
        for (int i = 0; i < gubo.numLights; i++) {
            gubo.pLights[i].position = internalLights[i].position;

            // --- FLICKERING ORGANICO DELLE TORCE ---
            // Combina onde sinusoidali e cosinusoidali a frequenze diverse più un micro-rumore
            // per simulare l'effetto realistico della fiamma.
            float t = totalTime + internalLights[i].randomOffset;
            float wave1 = std::sin(t * internalLights[i].freq1);
            float wave2 = std::cos(t * internalLights[i].freq2);
            float noise = std::sin(t * 45.0f) * 0.05f;

            float flicker = 0.85f + 0.12f * (wave1 * wave2) + noise;
            gubo.pLights[i].color = internalLights[i].baseColor * flicker;
        }
    }

    float getTimeOfDay() const { return timeOfDay; }

    // Selezione dinamica del modello di skydome da visualizzare in base agli orari (giorno, tramonto, notte)
    std::string getCurrentSkydomeInstanceId() const {
        if (timeOfDay >= 6.0f && timeOfDay < 17.0f) {
            return "skydome_day";
        } else if (timeOfDay >= 17.0f && timeOfDay < 20.0f) {
            return "skydome_sunset";
        } else if (timeOfDay >= 20.0f && timeOfDay < 23.0f) {
            return "skydome_night";
        } else if (timeOfDay >= 23.0f || timeOfDay < 4.0f) {
            return "skydome_night2";
        } else {
            return "skydome_night3";
        }
    }

    glm::vec4 getSkyColor() const { return currentSkyColor; }
    bool isTimeAccelerated() const { return timeSpeed > 1.0f; }
};