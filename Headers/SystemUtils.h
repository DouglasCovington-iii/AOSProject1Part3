#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <netdb.h>

// File I/O helpers
void writeAll(int fd, const void* buffer, std::size_t length);
ssize_t readAll(int fd, void* buffer, std::size_t length);

// Socket helpers
void sendAll(int sockfd, const void* buffer, std::size_t length);
void recvAll(int sockfd, void* buffer, std::size_t length);

// Control message helpers
void sendCMToFD(int sockfd, const std::string& msg);
std::string recvCMFromFD(int sockfd);

// Endianness helpers
std::uint64_t reverseBytes(std::uint64_t value);
std::uint64_t hostToNetwork64(std::uint64_t value);
std::uint64_t networkToHost64(std::uint64_t value);

// File utilities
bool isFileOpen(int file_fd);
std::uint64_t getFileSize(int file_fd);

// File transfer
void sendFileByteStream(int sockfd, const std::string& fileName);
void recvFileByteStream(int sockfd, std::string placeholderDir, std::string filename);

addrinfo dnsLookup(std::string hostname, int port_num);

#endif // NETWORK_UTILS_H
