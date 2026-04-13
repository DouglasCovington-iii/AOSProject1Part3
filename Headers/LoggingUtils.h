#ifndef LOGGING_H
#define LOGGING_H
#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sstream>

namespace Roles {
    constexpr const char* DRIVER    = "DRIVER";

    constexpr const char* INTIATOR  = "INTIATOR";
    constexpr const char* RECIPIENT = "RECIPIENT";
    
    constexpr const char* RECIVER   = "RECIVER";
    constexpr const char* UFDP      = "UFDP";
    constexpr const char* FDP       = "FDP";
}

namespace Layers {
    constexpr const char* SYSTEM    = "[SYSTEM] ";
    constexpr const char* FRAME  = "[FRAME] ";
    constexpr const char* BB    = "<BB> ";
    constexpr const char* CM  = "<CM> ";
    constexpr const char* FS  = "<FS> ";
}

class Logger {
private:
    const std::unordered_map<int, const char*> port_to_protocol = {
        {5000, Roles::UFDP},
        {5001, Roles::RECIPIENT},
        {5002, Roles::FDP}
    };

    bool logToFile;
    bool logToConsole;
    std::mutex atomic_print;
    std::mutex atomic_str_buff;

    std::fstream file_stream;
    std::ostringstream str_buff_s;

    void log_impl() {
        if (logToConsole) {
            std::cout << "\n";
        }

        if (logToFile) {
            file_stream << "\n";
            file_stream.flush();
        }
    }

    template <typename T, typename... Rest>
    void log_impl(const T& first, const Rest&... rest) {
        
        if (logToConsole) {
            std::cout << first;
        }

        if (logToFile) {
            file_stream << first;
        }

        log_impl(rest...);
    }

    void log_impl_str() {
        
    }

    template <typename T, typename... Rest>
    void log_impl_str(const T& first, const Rest&... rest) {
        
        str_buff_s << first;

        log_impl_str(rest...);
    }

public:
    std::string delimiter = "-------------------------------------------------------------";

    Logger();
    Logger(const std::string& file_name);

    void setUpFile(const std::string& file_name);
    void muteConsole();
    void unmuteConsole();

    void log() {
        std::lock_guard<std::mutex> lock(atomic_print);

        if (logToConsole) {
            std::cout << '\n';
        }

        if (logToFile) {
            file_stream << '\n';
        }
    }
    
    template <typename... Args>
    void log(const Args&... args) {
        std::lock_guard<std::mutex> lock(atomic_print);

        log_impl(args...);
    }

    template <typename... Args>
    void listenerLog(int port_num, const Args&... rest) {
        std::lock_guard<std::mutex> lock(atomic_print);

        std::string formatted_msg = std::string("(") + port_to_protocol.at(port_num) + " listener)"  + ": " ;
        log_impl(formatted_msg, rest...);
    }

    template <typename... Args>
    void handlerLog(const std::string& role, int t_id, const Args&... rest) {
        std::lock_guard<std::mutex> lock(atomic_print);

        std::string formatted_msg = "(" + role + " handler, t_id = " + std::to_string(t_id) + "): ";
        log_impl(formatted_msg, rest...);
    }

    template <typename... Args>
    void userHandlerLog(int t_id, const Args&... rest) {
        handlerLog(Roles::UFDP, t_id, rest...);
    }

    template <typename... Args>
    void reciverHandlerLog(int t_id, const Args&... rest) {
        handlerLog(Roles::RECIVER, t_id, rest...);
    }
    
    template <typename... Args>
    void fdpHandlerLog(int t_id, const Args&... rest) {
        handlerLog(Roles::FDP, t_id, rest...);
    }

    template <typename... Args>
    void recipientHandlerLog(int t_id, const Args&... rest) {
        handlerLog(Roles::RECIPIENT, t_id, rest...);
    }

    template <typename... Args>
    void initiatorHandlerLog(int t_id, const Args&... rest) {
        handlerLog(Roles::INTIATOR, t_id, rest...);
    }

    template <typename... Args>
    void logWithSpacing(int blanks_above, int blanks_below, const Args&... rest) {
        for (int i = 1; i <= blanks_above; i++) {
            log();
        }

        log(rest...);

        for (int i = 1; i <= blanks_below; i++) {
            log();
        }
    }

    template <typename... Args>
    void logWithSpacing(int spacing, const Args&... rest) {
        logWithSpacing(spacing, spacing, rest...);
    }

    template <typename... Args>
    void importantLog(const Args&... rest) {
        std::string delimiter = "-------------------------------------------------------------";

        logWithSpacing(1, 1, delimiter);
        log(rest...);
        logWithSpacing(1, 1, delimiter);
    }

    void logDelimiter() {
        log(delimiter);
    }

    void logDelimiter(int blanks_above, int blanks_below) {
        for (int i = 1; i <= blanks_above; i++) {
            log();
        }

        log(delimiter);

        for (int i = 1; i <= blanks_below; i++) {
            log();
        }
    }

    void logWriteStatus(int curr_sent, int total) {
        log("[Frame] Written ", curr_sent, " / ", total, " bytes");
    }

    void logReadStatus(int curr_recv, int total) {
        log("[Frame] Read ", curr_recv, " / ", total, " bytes");;
    }

    void logSendStatus(int curr_sent, int total) {
        log("[Frame] Sent ", curr_sent, " / ", total, " bytes");
    }

    void logRecvStatus(int curr_recv, int total) {
        log("[Frame] Recvived ", curr_recv, " / ", total, " bytes");;
    }

    template <typename... Args>
    void systemLog(const Args&... rest) {
        log("[System] ", rest...);
    }

    void logSendingFileStatus(int curr_sent, int total, std::string file_name) {
        log("[System] Sent ", curr_sent, " / ", total, " bytes of file ", file_name);
    }

    void logRecvingFileStatus(int curr_recv, int total, std::string file_name) {
        log("[System] Downloaded ", curr_recv, " / ", total, " bytes of file ", file_name);
    }

    template <typename... Args>
    void frameLog(const Args&... rest) {
        log("[Frame] ", rest...);
    }

    template <typename... Args>
    void bbLog(const Args&... rest) {
        log(Layers::BB, rest...);
    }

    template <typename... Args>
    void cmLog(const Args&... rest) {
        log(Layers::CM, rest...);
    }

    template <typename... Args>
    void fsLog(const Args&... rest) {
        log(Layers::FS, rest...);
    }

    template <typename... Args>
    std::string formatStr(const Args&... rest) {
        std::lock_guard<std::mutex> lock(atomic_str_buff);
        
        str_buff_s.str("");   // clear the internal buffer (the string)
        str_buff_s.clear();

        log_impl_str(rest...);
        return str_buff_s.str();
    }

    template <typename... Args>
    void logAndThrowError(const std::string& layer, const Args&... rest) {
        log(layer, rest...);
        std::string str = formatStr(layer, rest...);

        throw std::runtime_error(str);
    }


};


// void log();
// void log(const std::string& msg);



#endif