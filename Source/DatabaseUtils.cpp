#include <stdexcept>
#include <string>
#include <Config.h>
#include <fstream>
#include <mutex>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <LoggingUtils.h>

extern int LOCAL_ID;
extern std::mutex file_mtx;
extern Logger l;

std::string initLocalDatabase() {
    int id = LOCAL_ID;

    std::string baseDir = config::DATABASES_DIR;
    std::string targetDir = baseDir + "/" + (std::to_string(id) + "-Database");
    std::string templateDir = baseDir + "/" + "temp-Database";

    struct stat info;
    if (stat(targetDir.c_str(), &info) == 0) {
        return targetDir;
    }

    if (stat(templateDir.c_str(), &info) != 0) {
        throw std::runtime_error("Template database does not exist");
    }

    // THIS CAN BE REALLY BAD IF DETERMINTED BY USER INPUT
    std::string cmd = "cp -r " + templateDir + " " + targetDir;
    int result = system(cmd.c_str());

    if (result != 0) {
        throw std::runtime_error("Failed to copy intial database");
    }

    return targetDir;

    /*
    if Databases/{id}-Database file exists:
        return // do nothing, Database Directory created

    // Effectively do the system command

    System("cp /Databases/temp-Database /Databases/{id}-Database")

    //Is there a more portable way to do it?

    */
}


//assumes /Databases/{id}-Database exists
bool lookUpFile(const std::string& file_name) {
    std::lock_guard<std::mutex> lock(file_mtx);
    int id = LOCAL_ID;

    std::string path = config::DATABASES_DIR + "/" + std::to_string(id) + "-Database/index.txt";

    std::ifstream file(path);

    if (!file.is_open()) {
        l.log("Could not open ", path);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line == file_name) {
            l.log("Found ", file_name);
            return true; 
        }
    }

    l.log("Did not find '", file_name, "'");
    return false;
    /*
        if file_name is in /Databases/{id}-Database/Index.txt
            return true;
        else
            return false;
    */
}

void addFileToIndex(std::string file_name) {
    std::lock_guard<std::mutex> lock(file_mtx);
    int id = LOCAL_ID;

    if (lookUpFile(file_name)) {
        return;
    }

    std::string path = config::DATABASES_DIR + "/" + std::to_string(id) + "-Database/Index.txt";
    std::ofstream file(path);

    //Assuming that the file ends with a new line, else would place on the same
    file << file_name << "\n";

}

std::string getDatabaseDir() {
    int id = LOCAL_ID;

    return config::DATABASES_DIR + "/" + std::to_string(id) + "-Database";
}

std::string makeDbPath(std::string file_name) {
    return getDatabaseDir() + "/" + file_name;
}
