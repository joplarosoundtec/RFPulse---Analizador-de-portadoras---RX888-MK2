#include "core/Logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using rfpulse::core::Logger;

namespace {

std::string TempLogPath(const char* name)
{
    return (std::filesystem::temp_directory_path() / name).string();
}

std::string ReadWholeFile(const std::string& path)
{
    std::ifstream file(path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

} // namespace

TEST(Logger, WritesQueuedMessagesToFileBeforeDestruction)
{
    const std::string path = TempLogPath("rfpulse_test_logger.log");
    std::filesystem::remove(path);

    {
        Logger logger(path);
        logger.info("mensaje de info");
        logger.warning("mensaje de warning");
        logger.error("mensaje de error");
        logger.debug("mensaje de debug");
        // El destructor debe drenar la cola y cerrar el archivo antes de
        // devolver el control (log() es async, pero ~Logger() no lo es).
    }

    const std::string contents = ReadWholeFile(path);
    EXPECT_NE(contents.find("[INFO] mensaje de info"), std::string::npos);
    EXPECT_NE(contents.find("[WARNING] mensaje de warning"), std::string::npos);
    EXPECT_NE(contents.find("[ERROR] mensaje de error"), std::string::npos);
    EXPECT_NE(contents.find("[DEBUG] mensaje de debug"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(Logger, PreservesMessageOrder)
{
    const std::string path = TempLogPath("rfpulse_test_logger_order.log");
    std::filesystem::remove(path);

    {
        Logger logger(path);
        for (int i = 0; i < 50; ++i) {
            logger.info("linea " + std::to_string(i));
        }
    }

    const std::string contents = ReadWholeFile(path);
    std::size_t searchStart = 0;
    for (int i = 0; i < 50; ++i) {
        const std::string needle = "linea " + std::to_string(i);
        const std::size_t pos = contents.find(needle, searchStart);
        ASSERT_NE(pos, std::string::npos) << "no se encontro: " << needle;
        searchStart = pos + needle.size();
    }

    std::filesystem::remove(path);
}
