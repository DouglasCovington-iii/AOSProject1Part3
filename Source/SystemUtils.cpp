#include <SystemUtils.h>
#include <string>
#include <unistd.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <LoggingUtils.h>
#include <stdio.h>
#include <sstream>

extern Logger l;

void writeAll(int fd, const void* buffer, std::size_t length) {
    l.frameLog("Preparing to write ", length, "bytes to ", fd, " fd");

    const char* buf = static_cast<const char*>(buffer);
    std::size_t total_written = 0;

    l.logWriteStatus(total_written, length);

    int i = 1;
    while (total_written < length) {

        l.frameLog("pre_iteration=", i, 
                    ", fd=", fd,
                    ", total_sent=", total_written,
                    ", starting_address_of_buff=", (void *)(buf + total_written),
                    ", Maximum_amount_to_send=", length - total_written
        );

        ssize_t num_of_bytes_written = write(fd, buf + total_written, length - total_written);

        l.frameLog("post_iteration=", i,
                    ", fd=", fd,
                    ", num_of_bytes_written=", num_of_bytes_written);

        if (num_of_bytes_written <= 0) {
            l.logAndThrowError(Layers::FRAME, 
                "File Error while frameing fd ", fd, " on iteration ", i, 
                ", num_of_bytes_written=", num_of_bytes_written
            );
        }

        total_written += static_cast<std::size_t>(num_of_bytes_written);

        l.frameLog("post_iteration=", i,
                    ", fd=", fd,
                    ", total_written=", total_written
        );


        l.logWriteStatus(total_written, length);
        i++;
    }

    l.frameLog("Successfuly written ", length, "bytes into ", fd, " fd after ", i, " iterations");    
}


//THIS RETURNS WHAT WAS ACTULLY READ
ssize_t readAll(int fd, void* buffer, std::size_t length) {
    l.frameLog("Preparing to read ", length, " bytes from ", fd, " fd");

    char* buf = static_cast<char*>(buffer);
    std::size_t total_read = 0;

    int i = 1;
    l.logReadStatus(total_read, length);
    while(total_read < length) {

        l.frameLog("pre_iteration=", i, 
                    ", fd=", fd,
                    ", total_read=", total_read,
                    ", starting_address_of_buff=", (void *)(buf + total_read),
                    ", Maximum_amount_to_read=", length - total_read

        );

        ssize_t num_of_bytes_read = read(fd, buf + total_read, length - total_read);

        l.frameLog("post_iteration=", i, 
                    ", fd=", fd,
                    ", num_of_bytes_read=", num_of_bytes_read
        );

        if(num_of_bytes_read < 0) {
            l.logAndThrowError(Layers::FRAME, 
                "Error reading from fd ", fd, " on iteration ", i,
                ", num_of_bytes_read=", num_of_bytes_read
            );
        }

        if(num_of_bytes_read == 0) {
            l.frameLog("Partially read ", total_read , " / ", length, " bytes from ", fd, " fd");
            return total_read;
        }

        total_read += static_cast<std::size_t>(num_of_bytes_read);

        l.frameLog("post_iteration=", i, 
                    ", fd=", fd,
                    ", total_recv=", total_read
        );

        l.logReadStatus(total_read, length);
    }

    l.frameLog("Successfuly read ", total_read , " / ", length, " bytes from ", fd, " fd");
    return total_read;
}

void sendAll(int sockfd, const void* buffer, std::size_t length) {
    l.frameLog("Preparing to send ", length, " bytes from ", sockfd, " fd");

    const char* buf = static_cast<const char*>(buffer);
    std::size_t total_sent = 0;

    int i = 1;
    l.logSendStatus(total_sent, length);
    while (total_sent < length) {

        l.frameLog("pre_iteration=", i, 
                    ", sockfd=", sockfd,
                    ", total_sent=", total_sent,
                    ", starting_address_of_buff=", (void *)(buf + total_sent),
                    ", Maximum_amount_to_send=", length - total_sent
        );

        ssize_t sent = send(sockfd, buf + total_sent, length - total_sent, 0);

        l.frameLog("post_iteration=", i,
                    ", sockfd=", sockfd,
                    ", num_of_bytes_sent=", sent
        );

        if (sent < 0) {
            l.logAndThrowError(Layers::FRAME,
                "Socket Error with return code ", sent,
                ", iteration=", i
            );
        }

        if (sent == 0) {
            l.logAndThrowError(Layers::FRAME,
                "Socket prematurly closed ",
                ", iteration=", i,
                ", send=", send
            );
        }

        total_sent += static_cast<std::size_t>(sent);

        l.frameLog("post_iteration=", i,
                    ", sockfd=", sockfd,
                    ", total_sent=", total_sent
        );

        l.logSendStatus(total_sent, length);
        i++;
    }

    l.frameLog("Successfuly sent ", length, " bytes into ", sockfd, " fd");
}

void recvAll(int sockfd, void* buffer, std::size_t length) {
    l.frameLog("Preparing to recv ", length, " bytes from ", sockfd, " fd");

    char* buf = static_cast<char*>(buffer);
    std::size_t total_recv = 0;

    int i = 1;
    l.logRecvStatus(total_recv, length);
    while(total_recv < length) {

        l.frameLog("pre_iteration=", i, 
                    ", sockfd=", sockfd,
                    ", total_recv=", total_recv,
                    ", starting_address_of_buff=", (void *)(buf + total_recv),
                    ", Maximum_amount_to_recv=", length - total_recv

        );

        ssize_t num_of_bytes_recv = recv(sockfd, buf + total_recv, length - total_recv, 0);

        l.frameLog("post_iteration=", i, 
                    ", sockfd=", sockfd,
                    ", num_of_bytes_recv=", num_of_bytes_recv
        );

        if(num_of_bytes_recv < 0 ) {
            throw std::runtime_error("Socket error while framing " + std::to_string(sockfd) + " on iteration " + std::to_string(i));
        }

         if (num_of_bytes_recv == 0) {
            throw std::runtime_error("Premature Socket closure while framing " + std::to_string(sockfd) + " on iteration " + std::to_string(i));
        }

        total_recv += static_cast<std::size_t>(num_of_bytes_recv);

        l.frameLog("post_iteration=", i, 
                    ", sockfd=", sockfd,
                    ", total_recv=", total_recv
        );

        l.logRecvStatus(total_recv, length);
        i++;
    }

     l.frameLog("Successfuly recv ", length, "bytes from ", sockfd, " fd");
}

void sendCMToFD(int sockfd, const std::string& msg) {
    //l.log("Sending message '", msg, "' on fd ", sockfd);

    if (msg.size() == 0) {
        throw std::runtime_error("Must send a control message of non empty length");
    }

    l.systemLog("Preparing to send control message: '", msg, "'");

    l.systemLog("Sending size ", msg.size());
    uint32_t net_len = htonl(static_cast<uint32_t>(msg.size()));

    //l.log("Sending message size ", sizeof(msg.c_str()), " for  message '", msg, "' on fd ", sockfd);
    sendAll(sockfd, &net_len, sizeof(net_len));
    //l.log("Sending message data for message '", msg, "' on fd ", sockfd);

    l.systemLog("Preparing to send control message data");
    sendAll(sockfd, msg.data(), msg.size());

    // while (total < sizeof(net_len)) {
    //     ssize_t n = send(fd, len_ptr + total, sizeof(net_len) - total, 0);

    //     if (n <= 0) {
    //         throw std::runtime_error("Failed to send control message");
    //     }

    //     total += n;
    // }

    // total = 0;

    //  while (total < msg.size()) {
    //     ssize_t n = send(fd, msg.data() + total, msg.size() - total, 0);

    //     if (n <= 0) {
    //         throw std::runtime_error("Failed to send control message");
    //     }

    //     total += n;
    // }
}

std::string recvCMFromFD(int sockfd) {
    l.systemLog("Preparing to receive control message");

    uint32_t net_len;
    recvAll(sockfd, &net_len, sizeof(net_len));
    uint32_t len = ntohl(net_len);

    l.systemLog("Received length indicator: ", len);

    std::string result(len, '\0');

    if (len > 0) {
        recvAll(sockfd, &result[0], len);
    }

    l.systemLog("Recived control message: ", result);

    return result;

    // while (total < len) {
    //     ssize_t n = recv(fd, &result[0] + total, len - total, 0);

    //     if (n <= 0) {
    //         throw std::runtime_error("Malformed Control Message");
    //     }

    //     total += n;
    // }

    // while (total < sizeof(net_len)) {
    //     ssize_t n = recv(fd, len_ptr + total, sizeof(net_len) - total, 0);

    //     if (n <= 0) {
    //         throw std::runtime_error("Malformed Control Message");
    //     }

    //     total += n;
    // }

}

std::uint64_t reverseBytes(std::uint64_t value) {
    std::uint64_t result = 0;

        for (int i = 0; i < 8; ++i) {
            result = (result << 8) | (value & 0xFF);
            value >>= 8;
        }
        return result;
}
 
std::uint64_t hostToNetwork64(std::uint64_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return reverseBytes(value);
    #else
        return value;
    #endif

}

std::uint64_t networkToHost64(std::uint64_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return reverseBytes(value);
    #else
        return value;
    #endif
}

// MUTAL EXCLUSION NEEDS TO BE ENFORCED FOR FILE
bool isFileOpen(int file_fd) {
    return fcntl(file_fd, F_GETFD) != -1;
}

// MUTAL EXCLUSION NEEDS TO BE ENFORCED FOR FILE
//index/offset is not guarenteed to be perserved in case of errors
std::uint64_t getFileSize(int file_fd) {
    off_t cur_index = lseek64(file_fd, 0, SEEK_CUR);

    if (cur_index == -1) {
        throw std::runtime_error("lseek error");
    }

    off_t size = lseek64(file_fd, 0, SEEK_END);

    if (size == -1) {
        throw std::runtime_error("lseek error");
    }

    off_t prev = lseek64(file_fd, cur_index, SEEK_SET);

    if (prev == -1) {
        throw std::runtime_error("lseek error");
    }

    return static_cast<std::uint64_t>(size);

    // __off64_t cur_index = lseek64(file_fd, 0, SEEK_CUR);
    // __off64_t size = lseek64(file_fd, 0, SEEK_END);

    // __off64_t diff = size - cur_index;
    // //move back cursor to original postion
    // lseek64(file_fd, -diff, SEEK_END);

    // return size;

}

// MUTAL EXCLUSION NEEDS TO BE ENFORCED FOR FILE
void sendFileByteStream(int sockfd, const std::string& file_path) {
    l.systemLog("Preparing to send ", file_path);
    int file_fd = open(file_path.c_str(), O_RDONLY);

    if(file_fd == -1) {
        throw std::runtime_error("Could not open file " + file_path);
    }

    std::uint64_t file_size = getFileSize(file_fd);

    l.systemLog("Preparing to send size of file '", file_path, "': ", file_size, " bytes");
    std::uint64_t net_file_size = hostToNetwork64(file_size);

    sendAll(sockfd, &net_file_size, sizeof(std::uint64_t));

    std::uint64_t total_sent = 0;
    char buff[4096];

    l.systemLog("Preparing to send file content");

    l.logSendingFileStatus(total_sent, file_size, file_path);
    while (total_sent < file_size) {
        ssize_t num_of_bytes_read = read(file_fd, buff, 4096);

        if (num_of_bytes_read < 0) {
            throw std::runtime_error("Failed to read " + file_path);
        }

        if (num_of_bytes_read == 0) {
            throw std::runtime_error("Reached end of file prematurly");
        }

        sendAll(sockfd, buff, num_of_bytes_read);
        total_sent += static_cast<std::uint64_t>(num_of_bytes_read);

        l.logSendingFileStatus(total_sent, file_size, file_path);
    }

    close(file_fd);

    
    //sendAll(fd, reinterpret_cast<const char*)(&netFileSize), size(netFileSize))
}

void recvFileByteStream(int sockfd, std::string placeholderDir, std::string filename) {
    std::uint64_t net_len;
    recvAll(sockfd, &net_len, sizeof(std::uint64_t));
    std::uint64_t size_of_file = networkToHost64(net_len);

    std::string targetDir = placeholderDir + "/" + filename;
    int file_fd = open(targetDir.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (file_fd == -1) {
        throw std::runtime_error("Failed to open " + targetDir);
    }

    std::uint64_t total_recived = 0;
    const std::uint64_t CHUNK_SIZE = 4096;
    char buff[CHUNK_SIZE];

    l.logRecvingFileStatus(total_recived, size_of_file, filename);
    while(total_recived < size_of_file) {
        std::uint64_t num_of_bytes_left = size_of_file - total_recived;
        std::uint64_t buff_size = (num_of_bytes_left < CHUNK_SIZE ? num_of_bytes_left: CHUNK_SIZE);

        ssize_t recived = recv(sockfd, buff, buff_size, 0);

        if (recived < 0) {
            throw std::runtime_error("Error in connection downloading file");
        }

        if (recived == 0) {
            throw std::runtime_error("Connection closed before downloading file");
        }

        writeAll(file_fd, buff, recived);
        total_recived += static_cast<std::uint64_t>(recived);

        l.logRecvingFileStatus(total_recived, size_of_file, filename);
    }

    close(file_fd);
}

addrinfo dnsLookup(std::string hostname, int port_num) {
    std::string p = std::to_string(port_num);

    addrinfo hints{};
    addrinfo* res;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname.c_str(), p.c_str(), &hints, &res);

    if (status != 0) {
        throw std::runtime_error("Could not resolve DNS query");
    }

    return *res;

}

