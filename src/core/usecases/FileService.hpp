// Defines FileService -- loads and saves files, and converts paths to LSP URIs.

#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/entities/Document.hpp"

namespace editor::core::usecases {

class FileService {
public:
    // Reads the file at path into a new Document.
    // Throws std::runtime_error if the file cannot be opened.
    static Document load(const std::string& path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("cannot open file: " + path);
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return Document(ss.str());
    }

    // Writes the Document's buffer back to path.
    // Throws std::runtime_error if the file cannot be written.
    static void save(const Document& doc, const std::string& path) {
        std::ofstream file(path);
        if (!file) {
            throw std::runtime_error("cannot write file: " + path);
        }
        file << doc.buffer().to_string();
    }

    // Converts an absolute filesystem path to an LSP file URI.
    // e.g. "/home/user/foo.cpp" -> "file:///home/user/foo.cpp"
    static std::string path_to_uri(const std::string& path) {
        auto abs = std::filesystem::absolute(path).string();
        return "file://" + abs;
    }
};

}  // namespace editor::core::usecases
