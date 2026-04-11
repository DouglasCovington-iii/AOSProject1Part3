#include <LoggingUtils.h>
#include <mutex>
#include <iostream>

Logger::Logger() : logToFile(false), logToConsole(true) {}
Logger::Logger(const std::string &file_name): logToFile(true), logToConsole(true),
    file_stream(file_name) {}

void Logger::setUpFile(const std::string& file_name) {
    file_stream.open(file_name, std::ios::out);

    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open " + file_name);
}

    logToFile = true;

    log("Set up logging file at lcoation ", file_name);
}

void Logger::unmuteConsole() {
    logToConsole = true;
}

void Logger::muteConsole() {
    logToConsole = false;
}
