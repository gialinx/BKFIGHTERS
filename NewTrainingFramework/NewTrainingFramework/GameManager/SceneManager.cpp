#include "stdafx.h"
#include "SceneManager.h"
#include "../Core/Globals.h"
#include <fstream>
#include <iostream>
#include "GSPlay.h"

bool GSPlay_IsShowPlatformBoxes();

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SceneManager* SceneManager::s_instance = nullptr;

SceneManager* SceneManager::GetInstance() {
    if (!s_instance) {
        s_instance = new SceneManager();
    }
    return s_instance;
}

void SceneManager::DestroyInstance() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

SceneManager::SceneManager() 
    : m_activeCameraIndex(-1) {
    // Create default 2D camera
    CreateCamera();
    m_activeCameraIndex = 0;
}

SceneManager::~SceneManager() {
    Clear();
}

std::string SceneManager::GetSceneFileForState(StateType stateType) {
    switch (stateType) {
        case StateType::INTRO:
            return "../Resources/GSIntro.txt";
        case StateType::MENU:
            return "../Resources/GSMenu.txt";
        case StateType::PLAY:
            return "../Resources/GSPlay.txt";
        default:
            return "";
    }
}

bool SceneManager::LoadSceneForState(StateType stateType) {
    std::string filepath = GetSceneFileForState(stateType);
    if (filepath.empty()) {
        return false;
    }
    
    return LoadFromFile(filepath);
}

bool SceneManager::ParseCameraConfig(std::ifstream& file, const std::string& line) {
    std::string configLine;
    
    // Parse camera configuration
    while (std::getline(file, configLine)) {
        if (configLine.empty() || configLine[0] == '#') {
            // If we hit another section or end, put the line back by seeking
            std::streampos pos = file.tellg();
            file.seekg(pos - (std::streamoff)(configLine.length() + 1));
            break;
        }
        
        if (configLine.find("POSITION") == 0) {
            sscanf(configLine.c_str(), "POSITION %f %f %f", 
                &m_cameraConfig.position.x, &m_cameraConfig.position.y, &m_cameraConfig.position.z);
        }
        else if (configLine.find("TARGET") == 0) {
            sscanf(configLine.c_str(), "TARGET %f %f %f", 
                &m_cameraConfig.target.x, &m_cameraConfig.target.y, &m_cameraConfig.target.z);
        }
        else if (configLine.find("UP") == 0) {
            // Ignore UP vector input, force 2D up vector (0,1,0)
            m_cameraConfig.up = Vector3(0.0f, 1.0f, 0.0f);
        }
        else if (configLine.find("NEAR") == 0) {
            sscanf(configLine.c_str(), "NEAR %f", &m_cameraConfig.nearPlane);
        }
        else if (configLine.find("FAR") == 0) {
            sscanf(configLine.c_str(), "FAR %f", &m_cameraConfig.farPlane);
        }
        else if (configLine.find("LEFT") == 0) {
            sscanf(configLine.c_str(), "LEFT %f", &m_cameraConfig.left);
        }
        else if (configLine.find("RIGHT") == 0) {
            sscanf(configLine.c_str(), "RIGHT %f", &m_cameraConfig.right);
        }
        else if (configLine.find("BOTTOM") == 0) {
            sscanf(configLine.c_str(), "BOTTOM %f", &m_cameraConfig.bottom);
        }
        else if (configLine.find("TOP") == 0) {
            sscanf(configLine.c_str(), "TOP %f", &m_cameraConfig.top);
        }
    }
    
    return true;
}

void SceneManager::SetupCameraFromConfig() {
    Camera* activeCamera = GetActiveCamera();
    if (!activeCamera) {
        CreateCamera();
        activeCamera = GetActiveCamera();
    }
    
    if (activeCamera) {
        // Set camera position and look direction (2D)
        activeCamera->SetLookAt(m_cameraConfig.position, m_cameraConfig.target, m_cameraConfig.up);
        
        // 2D-only: Always use orthographic projection
        activeCamera->SetOrthographic(
            m_cameraConfig.left, m_cameraConfig.right,
            m_cameraConfig.bottom, m_cameraConfig.top,
            m_cameraConfig.nearPlane, m_cameraConfig.farPlane
        );
    }
}

bool SceneManager::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "Cannot open SceneManager file: " << filepath << std::endl;
        return false;
    }

    RemoveAllObjects();
    
    std::string line;
    int objectCount = 0;
    bool hasCameraConfig = false;

    // First pass - look for camera config and object count
    while (std::getline(file, line)) {
        if (line.find("#Camera") != std::string::npos) {
            ParseCameraConfig(file, line);
            hasCameraConfig = true;
        }
        else if (line.find("#ObjectCount") != std::string::npos) {
            // Extract number from the same line: "#ObjectCount 4"
            size_t spacePos = line.find(' ');
            if (spacePos != std::string::npos) {
                std::string countStr = line.substr(spacePos + 1);
                // Trim whitespace
                countStr.erase(0, countStr.find_first_not_of(" \t\r\n"));
                countStr.erase(countStr.find_last_not_of(" \t\r\n") + 1);
                
                try {
                    if (!countStr.empty()) {
                        objectCount = std::stoi(countStr);
                    } else {
                        objectCount = 0;
                    }
                } catch (const std::invalid_argument& e) {
                    objectCount = 0;
                } catch (const std::out_of_range& e) {
                    objectCount = 0;
                }
            } else {
                objectCount = 0;
            }
            break;
        }
    }
    
    // Setup camera if config was found
    if (hasCameraConfig) {
        SetupCameraFromConfig();
    }
    
    // Load objects  
    for (int i = 0; i < objectCount; ++i) {
        Object* obj = nullptr;
        int id = -1;
        int modelId = -1, shaderId = -1;
        std::vector<int> textureIds;
        Vector3 position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1);
        bool objectDataComplete = false;

        while (std::getline(file, line) && !objectDataComplete) {
            if (line.find("ID") == 0) {
                sscanf(line.c_str(), "ID %d", &id);
                obj = CreateObject(id);
            }
            else if (line.find("MODEL_ID") == 0) {
                sscanf(line.c_str(), "MODEL_ID %d", &modelId);
            }
            else if (line.find("TEXTURE_ID") == 0) {
                int textureId;
                sscanf(line.c_str(), "TEXTURE_ID %d", &textureId);
                textureIds.push_back(textureId);
            }
            else if (line.find("SHADER_ID") == 0) {
                sscanf(line.c_str(), "SHADER_ID %d", &shaderId);
            }
            else if (line.find("POS") == 0) {
                sscanf(line.c_str(), "POS %f %f %f", &position.x, &position.y, &position.z);
            }
            else if (line.find("ROTATION") == 0) {
                sscanf(line.c_str(), "ROTATION %f %f %f", &rotation.x, &rotation.y, &rotation.z);
            }
            else if (line.find("SCALE") == 0) {
                sscanf(line.c_str(), "SCALE %f %f %f", &scale.x, &scale.y, &scale.z);
                objectDataComplete = true; // SCALE is the last field for each object
            }
        }
        
        // Setup object if created successfully
        if (obj) {
            obj->SetPosition(position);
            obj->SetRotation(rotation);
            obj->SetScale(scale);
            
            if (modelId >= 0) {
                obj->SetModel(modelId);
            }
            
            for (int j = 0; j < (int)textureIds.size(); ++j) {
                obj->SetTexture(textureIds[j], j);
            }
            
            if (shaderId >= 0) {
                obj->SetShader(shaderId);
            }
        }

    }
    
    file.close();
    return true;
}

Object* SceneManager::CreateObject(int id) {
    auto obj = std::make_unique<Object>(id);
    Object* objPtr = obj.get();
    m_objects.push_back(std::move(obj));
    return objPtr;
}

Object* SceneManager::GetObject(int id) {
    for (auto& obj : m_objects) {
        if (obj->GetId() == id) {
            return obj.get();
        }
    }
    return nullptr;
}

void SceneManager::RemoveObject(int id) {
    auto it = std::remove_if(m_objects.begin(), m_objects.end(),
        [id](const std::unique_ptr<Object>& obj) {
            return obj->GetId() == id;
        });
    
    if (it != m_objects.end()) {
        m_objects.erase(it, m_objects.end());
    }
}

void SceneManager::RemoveAllObjects() {
    m_objects.clear();
}

Camera* SceneManager::CreateCamera() {
    auto camera = std::make_unique<Camera>();
    // Set default 2D orthographic camera
    float aspect = (float)Globals::screenWidth / (float)Globals::screenHeight;
    camera->SetOrthographic(-aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
    camera->SetLookAt(Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
    
    Camera* cameraPtr = camera.get();
    m_cameras.push_back(std::move(camera));
    return cameraPtr;
}

Camera* SceneManager::GetActiveCamera() {
    if (m_activeCameraIndex >= 0 && m_activeCameraIndex < (int)m_cameras.size()) {
        return m_cameras[m_activeCameraIndex].get();
    }
    return nullptr;
}

Camera* SceneManager::GetCamera(int index) {
    if (index >= 0 && index < (int)m_cameras.size()) {
        return m_cameras[index].get();
    }
    return nullptr;
}

void SceneManager::SetActiveCamera(int index) {
    if (index >= 0 && index < (int)m_cameras.size()) {
        m_activeCameraIndex = index;
    }
}

void SceneManager::Update(float deltaTime) {
    for (auto& obj : m_objects) {
        obj->Update(deltaTime);
    }
}

void SceneManager::Draw() {
    Camera* activeCamera = GetActiveCamera();
    if (!activeCamera) {
        return;
    }
    
    const Matrix& viewMatrixRef = activeCamera->GetViewMatrix();
    const Matrix& projMatrixRef = activeCamera->GetProjectionMatrix();
    Matrix viewMatrix;
    Matrix projectionMatrix;
    viewMatrix = const_cast<Matrix&>(viewMatrixRef);
    projectionMatrix = const_cast<Matrix&>(projMatrixRef);
    
    // UI matrices independent of camera zoom/movement (screen-space)
    float aspect = (float)Globals::screenWidth / (float)Globals::screenHeight;
    Matrix uiViewMatrix;
    Matrix uiProjectionMatrix;
    uiViewMatrix.SetLookAt(Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
    uiProjectionMatrix.SetOrthographic(-aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
    
    // Draw all objects except character objects (ID 1000, 1001)
    // Character objects will be drawn by Character class
    for (auto& obj : m_objects) {
        int objId = obj->GetId();
        if (objId != 1000 && objId != 1001) {
            // Platform boxes
            if ((objId >= 500 && objId < 600)) {
                if (GSPlay_IsShowPlatformBoxes()) {
                    obj->Draw(viewMatrix, projectionMatrix);
                }
            }
            // Wall boxes
            else if ((objId >= 400 && objId < 500)) {
                if (GSPlay::IsShowWallBoxes()) {
                    obj->Draw(viewMatrix, projectionMatrix);
                }
            }
            // Ladder boxes
            else if ((objId >= 600 && objId < 700)) {
                if (GSPlay::IsShowLadderBoxes()) {
                    obj->Draw(viewMatrix, projectionMatrix);
                }
            }
            // Teleport boxes
            else if ((objId >= 700 && objId < 800)) {
                if (GSPlay::IsShowTeleportBoxes()) {
                    obj->Draw(viewMatrix, projectionMatrix);
                }
            }
            // HUD/UI (screen-space, fixed position regardless of camera zoom/move)
            else if ((objId >= 900 && objId < 1000)) {
                if (objId == 916 || objId == 917) {
                    continue;
                }
                obj->Draw(uiViewMatrix, uiProjectionMatrix);
            }
            else if ((objId >= 976 && objId <= 979)) {
                continue;
            }
            else {
                obj->Draw(viewMatrix, projectionMatrix);
            }
        }
    }
}

void SceneManager::HandleInput(unsigned char key, bool isPressed) {
    if (!isPressed) return;
    
    Camera* activeCamera = GetActiveCamera();
    if (!activeCamera) return;
    
    // Simplified 2D controls only
    switch(key) {
        case 'R':
        case 'r':
            break;
    }
}

void SceneManager::Clear() {
    RemoveAllObjects();
    m_cameras.clear();
    m_activeCameraIndex = -1;
}

