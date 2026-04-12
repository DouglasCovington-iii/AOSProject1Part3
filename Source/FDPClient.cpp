#include <stdexcept>
#include <SystemUtils.h>
#include <netdb.h>
#include <Config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <LoggingUtils.h>

Logger l;

// struct DatabaseResponse {
//     int fd;
//     std::string target_dir;
// };

// DatabaseResponse initLocalDatabase(std::string file_name) {
//     std::string baseDir = config::DATABASES_DIR;
//     std::string targetDir = baseDir + "/" + (file_name);

//     int fd = open(targetDir.c_str(), O_RDWR, "644");

//     DatabaseResponse response;

//     response.fd = fd;
//     response.target_dir = targetDir;

//     return response;
// }

std::string lookupMachineAddress(int id) {
    if (id <= 0) {
        throw std::invalid_argument("ID must be >= 1");
    }

    std::string target_dir = config::CONFIG_DIR + "/" + "IdMapping.txt";
    std::ifstream file(target_dir);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file " + target_dir);
    }

    std::string line;
    int current = 1;

    while (std::getline(file, line)) {
        if (current == id) {
            return line;
        }
        current++;
    }

    throw std::out_of_range("Line number exceeds file length");
}


int main(int argc, char** argv) {

    if (argc != 3) {
        throw std::invalid_argument(std::string("Invalid number of arguments\n") + std::string("gave ") + std::to_string(argc) + ", expected 2");
    }

    int recp_id = std::stoi(argv[1]);
    std::string file_req = argv[2];

    l.log("Requesting file '", file_req, "' from machine ", recp_id);

    std::string hostname = lookupMachineAddress(recp_id);
    addrinfo result = dnsLookup(hostname, 5002);
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    int rc = connect(sock_fd, result.ai_addr, result.ai_addrlen);

    if (rc == -1) {
        throw std::runtime_error("Couldn't connect to " + hostname);
    }

    l.log("Successfully connected to machine ", recp_id);
    l.log("Preparing to send File Request: '", file_req, "'");

    sendCMToFD(sock_fd, file_req);

    l.log("Preparing for response");
    std::string response = recvCMFromFD(sock_fd);

    l.log("Received response '", response, "'");

    if (response != "SUCCESS") {
        throw std::runtime_error(response);
    }

    std::string base_dir = "UserDatabase";
    l.log("Preparing for file download; Placing in ", base_dir, "/", file_req);
    recvFileByteStream(sock_fd, base_dir, file_req);

    l.log("yaaay");





    // string -> connection address structure


}   
