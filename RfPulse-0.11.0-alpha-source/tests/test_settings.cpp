#include "core/Settings.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

using rfpulse::core::Settings;

namespace {

std::string TempSettingsPath(const char* name)
{
    return (std::filesystem::temp_directory_path() / name).string();
}

} // namespace

TEST(Settings, LoadOnMissingFileReturnsFalseButDoesNotThrow)
{
    const std::string path = TempSettingsPath("rfpulse_test_settings_missing.txt");
    std::filesystem::remove(path);

    Settings settings(path);
    EXPECT_FALSE(settings.load());

    EXPECT_DOUBLE_EQ(settings.getDouble("anything", 42.0), 42.0);
}

TEST(Settings, SaveThenLoadRoundTripsAllTypes)
{
    const std::string path = TempSettingsPath("rfpulse_test_settings_roundtrip.txt");
    std::filesystem::remove(path);

    {
        Settings settings(path);
        settings.setDouble("center_frequency_hz", 560123456.789);
        settings.setInt("fft_size", 16384);
        settings.setBool("max_hold_enabled", true);
        settings.setBool("min_hold_enabled", false);
        settings.setString("device_name", "RX888 mkII");
        ASSERT_TRUE(settings.save());
    }

    Settings reloaded(path);
    ASSERT_TRUE(reloaded.load());

    EXPECT_DOUBLE_EQ(reloaded.getDouble("center_frequency_hz", 0.0), 560123456.789);
    EXPECT_EQ(reloaded.getInt("fft_size", 0), 16384);
    EXPECT_TRUE(reloaded.getBool("max_hold_enabled", false));
    EXPECT_FALSE(reloaded.getBool("min_hold_enabled", true));
    EXPECT_EQ(reloaded.getString("device_name", ""), "RX888 mkII");

    std::filesystem::remove(path);
}

TEST(Settings, MissingKeyReturnsProvidedDefault)
{
    const std::string path = TempSettingsPath("rfpulse_test_settings_defaults.txt");
    std::filesystem::remove(path);

    Settings settings(path);
    settings.setInt("only_this_key", 7);
    settings.save();

    Settings reloaded(path);
    reloaded.load();

    EXPECT_EQ(reloaded.getInt("only_this_key", 0), 7);
    EXPECT_EQ(reloaded.getInt("does_not_exist", -1), -1);
    EXPECT_DOUBLE_EQ(reloaded.getDouble("does_not_exist_either", 3.5), 3.5);

    std::filesystem::remove(path);
}

TEST(Settings, IgnoresCommentsAndBlankLines)
{
    const std::string path = TempSettingsPath("rfpulse_test_settings_comments.txt");
    {
        std::ofstream file(path, std::ios::trunc);
        file << "# esto es un comentario\n";
        file << "\n";
        file << "volume = 0.75\n";
        file << "   # otro comentario con espacios delante\n";
    }

    Settings settings(path);
    ASSERT_TRUE(settings.load());
    EXPECT_DOUBLE_EQ(settings.getDouble("volume", 0.0), 0.75);

    std::filesystem::remove(path);
}
