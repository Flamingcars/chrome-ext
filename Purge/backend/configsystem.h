#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace ConfigSystem {
    struct Vector3 { float x, y, z; };
    struct Vector4 { float x, y, z, w; };
    struct ImVec4 { float x, y, z, w; };

    using ConfigValue = std::variant<bool*, int*, float*, double*, std::string*, Vector3*, Vector4*, std::vector<int>*, float(*)[3], float(*)[4], ImVec4*>;

    void AddKeybind(const std::string& keyName, const std::string& typeName, int* key, int* type);

    void AddGlobalValue(const std::string& key, bool* value);
    void AddGlobalValue(const std::string& key, int* value);
    void AddGlobalValue(const std::string& key, float* value);
    void AddGlobalValue(const std::string& key, double* value);
    void AddGlobalValue(const std::string& key, std::string* value);
    void AddGlobalValue(const std::string& key, Vector3* value);
    void AddGlobalValue(const std::string& key, Vector4* value);
    void AddGlobalValue(const std::string& key, std::vector<int>* value);
    void AddGlobalValue(const std::string& key, float (*value)[3]);
    void AddGlobalValue(const std::string& key, float (*value)[4]);
    void AddGlobalValue(const std::string& key, ImVec4* value);

    void Initialize();
    void RefreshConfigList();
    bool CreateConfig(const std::string& configName);
    bool SaveConfig(const std::string& configName);
    bool LoadConfig(const std::string& configName);
    void SetAutoloadConfig(const std::string& configName);
    void SaveAutoloadSetting();
    void LoadAutoloadSetting();

    const std::vector<std::string>& GetConfigList();
    const std::string& GetAutoloadConfig();
}