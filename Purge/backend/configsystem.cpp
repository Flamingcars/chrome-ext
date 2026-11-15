#include "ConfigSystem.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace ConfigSystem {
    std::unordered_map<std::string, ConfigValue> globalValues;
    std::vector<std::string> configList;
    std::string autoloadConfig = "None";
    std::string configDirectory;

    void AddGlobalValue(const std::string& key, bool* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, int* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, float* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, double* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, std::string* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, Vector3* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, Vector4* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, std::vector<int>* value) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, float (*value)[3]) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, float (*value)[4]) {
        globalValues[key] = value;
    }

    void AddGlobalValue(const std::string& key, ImVec4* value) {
        globalValues[key] = value;
    }

    void AddKeybind(const std::string& keyName, const std::string& typeName, int* key, int* type) {
        globalValues[keyName] = key;
        globalValues[typeName] = type;
    }

    void Initialize() {
        char* appdata = nullptr;
        size_t len = 0;
        _dupenv_s(&appdata, &len, "APPDATA");

        if (appdata != nullptr) {
            configDirectory = std::string(appdata) + "\\Centrum\\configs\\";
            free(appdata);
        }
        else {
            configDirectory = "configs\\";
        }

        std::filesystem::create_directories(configDirectory);
        RefreshConfigList();
        LoadAutoloadSetting();

        if (!autoloadConfig.empty() && autoloadConfig != "None") {
            LoadConfig(autoloadConfig);
        }
    }

    void RefreshConfigList() {
        configList.clear();

        try {
            for (const auto& entry : std::filesystem::directory_iterator(configDirectory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".centrum") {
                    std::string filename = entry.path().stem().string();
                    configList.push_back(filename);
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error refreshing config list: " << e.what() << std::endl;
        }
    }

    bool CreateConfig(const std::string& configName) {
        if (configName.empty()) {
            std::cerr << "Config name cannot be empty!" << std::endl;
            return false;
        }

        std::string filepath = configDirectory + configName + ".centrum";

        if (std::filesystem::exists(filepath)) {
            std::cerr << "Config '" << configName << "' already exists!" << std::endl;
            return false;
        }

        return SaveConfig(configName);
    }

    bool SaveConfig(const std::string& configName) {
        std::string filepath = configDirectory + configName + ".centrum";

        try {
            std::ofstream file(filepath);
            if (!file.is_open()) {
                std::cerr << "Failed to create config file: " << filepath << std::endl;
                return false;
            }

            for (const auto& [key, value] : globalValues) {
                file << key << "=";
                std::visit([&file](const auto& valuePtr) {
                    using T = std::decay_t<decltype(valuePtr)>;
                    if constexpr (std::is_same_v<T, Vector3*>) {
                        file << valuePtr->x << "," << valuePtr->y << "," << valuePtr->z;
                    }
                    else if constexpr (std::is_same_v<T, Vector4*>) {
                        file << valuePtr->x << "," << valuePtr->y << "," << valuePtr->z << "," << valuePtr->w;
                    }
                    else if constexpr (std::is_same_v<T, ImVec4*>) {
                        file << valuePtr->x << "," << valuePtr->y << "," << valuePtr->z << "," << valuePtr->w;
                    }
                    else if constexpr (std::is_same_v<T, std::vector<int>*>) {
                        for (size_t i = 0; i < valuePtr->size(); ++i) {
                            if (i > 0) file << ",";
                            file << (*valuePtr)[i];
                        }
                    }
                    else if constexpr (std::is_same_v<T, float(*)[3]>) {
                        file << (*valuePtr)[0] << "," << (*valuePtr)[1] << "," << (*valuePtr)[2];
                    }
                    else if constexpr (std::is_same_v<T, float(*)[4]>) {
                        file << (*valuePtr)[0] << "," << (*valuePtr)[1] << "," << (*valuePtr)[2];
                    }
                    else {
                        file << *valuePtr;
                    }
                    }, value);
                file << std::endl;
            }

            file.close();
            std::cout << "Config '" << configName << "' saved successfully!" << std::endl;
            RefreshConfigList();
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving config: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadConfig(const std::string& configName) {
        std::string filepath = configDirectory + configName + ".centrum";

        if (!std::filesystem::exists(filepath)) {
            std::cerr << "Config '" << configName << "' does not exist!" << std::endl;
            return false;
        }

        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                std::cerr << "Failed to open config file: " << filepath << std::endl;
                return false;
            }

            std::string line;
            while (std::getline(file, line)) {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);

                    if (globalValues.find(key) != globalValues.end()) {
                        std::visit([&value](auto& valuePtr) {
                            using T = std::decay_t<decltype(valuePtr)>;
                            if constexpr (std::is_same_v<T, bool*>) {
                                *valuePtr = (value == "1" || value == "true");
                            }
                            else if constexpr (std::is_same_v<T, int*>) {
                                *valuePtr = std::stoi(value);
                            }
                            else if constexpr (std::is_same_v<T, float*>) {
                                *valuePtr = std::stof(value);
                            }
                            else if constexpr (std::is_same_v<T, double*>) {
                                *valuePtr = std::stod(value);
                            }
                            else if constexpr (std::is_same_v<T, std::string*>) {
                                *valuePtr = value;
                            }
                            else if constexpr (std::is_same_v<T, Vector3*>) {
                                std::istringstream ss(value);
                                std::string token;
                                if (std::getline(ss, token, ',')) valuePtr->x = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->y = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->z = std::stof(token);
                            }
                            else if constexpr (std::is_same_v<T, Vector4*>) {
                                std::istringstream ss(value);
                                std::string token;
                                if (std::getline(ss, token, ',')) valuePtr->x = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->y = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->z = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->w = std::stof(token);
                            }
                            else if constexpr (std::is_same_v<T, ImVec4*>) {
                                std::istringstream ss(value);
                                std::string token;
                                if (std::getline(ss, token, ',')) valuePtr->x = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->y = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->z = std::stof(token);
                                if (std::getline(ss, token, ',')) valuePtr->w = std::stof(token);
                            }
                            else if constexpr (std::is_same_v<T, std::vector<int>*>) {
                                valuePtr->clear();
                                std::istringstream ss(value);
                                std::string token;
                                while (std::getline(ss, token, ',')) {
                                    valuePtr->push_back(std::stoi(token));
                                }
                            }
                            else if constexpr (std::is_same_v<T, float(*)[3]>) {
                                std::istringstream ss(value);
                                std::string token;
                                if (std::getline(ss, token, ',')) (*valuePtr)[0] = std::stof(token);
                                if (std::getline(ss, token, ',')) (*valuePtr)[1] = std::stof(token);
                                if (std::getline(ss, token, ',')) (*valuePtr)[2] = std::stof(token);
                            }
                            else if constexpr (std::is_same_v<T, float(*)[4]>) {
                                std::istringstream ss(value);
                                std::string token;
                                if (std::getline(ss, token, ',')) (*valuePtr)[0] = std::stof(token);
                                if (std::getline(ss, token, ',')) (*valuePtr)[1] = std::stof(token);
                                if (std::getline(ss, token, ',')) (*valuePtr)[2] = std::stof(token);
                                if (std::getline(ss, token, ',')) (*valuePtr)[3] = std::stof(token);
                            }
                            }, globalValues[key]);
                    }
                }
            }

            file.close();
            std::cout << "Config '" << configName << "' loaded successfully!" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            return false;
        }
    }

    void SetAutoloadConfig(const std::string& configName) {
        autoloadConfig = configName;
        SaveAutoloadSetting();
    }

    void SaveAutoloadSetting() {
        std::ofstream file(configDirectory + "autoload.txt");
        if (file.is_open()) {
            file << autoloadConfig;
            file.close();
        }
    }

    void LoadAutoloadSetting() {
        std::ifstream file(configDirectory + "autoload.txt");
        if (file.is_open()) {
            std::getline(file, autoloadConfig);
            file.close();
        }
    }

    const std::vector<std::string>& GetConfigList() {
        return configList;
    }

    const std::string& GetAutoloadConfig() {
        return autoloadConfig;
    }
}