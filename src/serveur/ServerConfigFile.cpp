#include "detail/Engine.hpp"
#include <Logger.hpp>
#include <fstream>
#include <algorithm>

namespace servd
{

    static Logger::LogLevel parse_log_level(const std::string& s) {
        if (s == "DEBUG") return Logger::LogLevel::DEBUG;
        if (s == "INFO")  return Logger::LogLevel::INFO;
        if (s == "WARN")  return Logger::LogLevel::WARN;
        if (s == "ERROR") return Logger::LogLevel::ERROR;
        return Logger::LogLevel::INFO;
    }

    static std::string trim(std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
        return s;
    }

    Server& Server::load_config(const std::string& path) {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Impossible d'ouvrir le fichier de config: " + path);

        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            const std::string key = trim(line.substr(0, eq));
            const std::string value = trim(line.substr(eq + 1));

            if (key == "tcp")       enable_tcp(static_cast<uint16_t>(std::stoi(value)));
            else if (key == "udp")  enable_udp(static_cast<uint16_t>(std::stoi(value)));
            else if (key == "unix") enable_unix_socket(value);
            else if (key == "max_clients") set_max_clients(static_cast<size_t>(std::stoul(value)));
            else if (key == "log_level") Logger::setLevel(parse_log_level(value));
            else if (key == "log_file")  Logger::setLogFile(value);
        }
        return *this;
    }

}
