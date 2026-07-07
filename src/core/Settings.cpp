#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

namespace rfpulse::core {

namespace {

std::string trim(const std::string& s)
{
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

} // namespace

Settings::Settings(std::string filePath)
    : filePath_(std::move(filePath))
{
}

bool Settings::load()
{
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        return false;
    }

    values_.clear();
    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(trimmed.substr(0, eq));
        const std::string value = trim(trimmed.substr(eq + 1));
        if (!key.empty()) {
            values_[key] = value;
        }
    }
    return true;
}

bool Settings::save() const
{
    std::ofstream file(filePath_, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "# RfPulse settings -- generado automaticamente, editable a mano.\n";
    for (const auto& [key, value] : values_) {
        file << key << " = " << value << "\n";
    }
    return true;
}

double Settings::getDouble(const std::string& key, double defaultValue) const
{
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return defaultValue;
    }
    try {
        return std::stod(it->second);
    } catch (...) {
        return defaultValue;
    }
}

int Settings::getInt(const std::string& key, int defaultValue) const
{
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return defaultValue;
    }
    int result = defaultValue;
    const auto [ptr, ec] = std::from_chars(it->second.data(), it->second.data() + it->second.size(), result);
    return (ec == std::errc()) ? result : defaultValue;
}

bool Settings::getBool(const std::string& key, bool defaultValue) const
{
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return defaultValue;
    }
    return it->second == "true" || it->second == "1";
}

std::string Settings::getString(const std::string& key, const std::string& defaultValue) const
{
    const auto it = values_.find(key);
    return (it != values_.end()) ? it->second : defaultValue;
}

void Settings::setDouble(const std::string& key, double value)
{
    values_[key] = std::to_string(value);
}

void Settings::setInt(const std::string& key, int value)
{
    values_[key] = std::to_string(value);
}

void Settings::setBool(const std::string& key, bool value)
{
    values_[key] = value ? "true" : "false";
}

void Settings::setString(const std::string& key, const std::string& value)
{
    values_[key] = value;
}

} // namespace rfpulse::core
