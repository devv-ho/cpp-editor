#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/usecases/FileService.hpp"

using namespace editor::core::usecases;

namespace {

// Writes content to a temp file and returns its path.
std::string write_temp(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / "editor_test_fileservice.cpp";
    std::ofstream f(path);
    f << content;
    return path.string();
}

}  // namespace

TEST(FileServiceTest, LoadReadsFileIntoDocument) {
    std::string content = "int main() {\n    return 0;\n}\n";
    auto path = write_temp(content);

    auto doc = FileService::load(path);
    EXPECT_EQ(doc.buffer().to_string(), content);
}

TEST(FileServiceTest, LoadThrowsOnMissingFile) {
    EXPECT_THROW(FileService::load("/nonexistent/path/file.cpp"), std::runtime_error);
}

TEST(FileServiceTest, SaveWritesBufferToFile) {
    std::string content = "void foo() {}\n";
    auto path = write_temp(content);

    auto doc = FileService::load(path);
    auto save_path = std::filesystem::temp_directory_path() / "editor_test_save_out.cpp";
    FileService::save(doc, save_path.string());

    std::ifstream f(save_path);
    std::ostringstream ss;
    ss << f.rdbuf();
    EXPECT_EQ(ss.str(), content);
}

TEST(FileServiceTest, PathToUriProducesFileUri) {
    auto path = write_temp("x");
    auto uri = FileService::path_to_uri(path);
    EXPECT_TRUE(uri.starts_with("file:///"));
    EXPECT_NE(uri.find(std::filesystem::absolute(path).string()), std::string::npos);
}
