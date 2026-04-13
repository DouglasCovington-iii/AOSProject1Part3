#include <string>
#include <ProtocolParserUtils.h>
#include <sstream>

std::ostream& operator<<(std::ostream& os, const RequestMsgPayload& rq_p) {
   os << "{" << "req_num=" << rq_p.reqNum << 
    ", file_name=" << rq_p.fileName << 
    ", ttl=" << rq_p.ttl << 
    ", source_path="  << formatNeighboorList(rq_p.sourcePath) << 
    "}";

    return os;
}   

std::ostream& operator<<(std::ostream& os, const ReplyMsgPayload& rp_p) {
    os << "{" << "req_num=" << rp_p.reqNum << 
    ", responder=" << rp_p.responder << 
    ", source_path=" << formatNeighboorList(rp_p.sourcePath) <<
    "}";

    return os;
}

bool isError(std::string cntrl_msg) {
    if (cntrl_msg.find("ERROR") != std::string::npos) {
        return true;
    }

    return false;
} 

std::string getLastComponentInPath(std::string path) {
    size_t pos = path.find_last_of("/\\");

    if (pos == std::string::npos) {
        return path;
    }

    return path.substr(pos + 1);
}

std::string formatNeighboorList(std::vector<int> neig_list) {
    std::ostringstream oss;

    oss << "[";
    for(int i = 0; i < neig_list.size(); i++) {
        oss << i << (i != (neig_list.size() - 1)? " ":"]");
    }

    return oss.str();
}

std::string getStem(std::string cntrl_msg) {
    size_t pos = cntrl_msg.find(' ');
    std::string cntrl_msg_stem = cntrl_msg.substr(0, pos);

    return cntrl_msg_stem;
}

RequestMsgPayload parseRequstCtrlMsg(const std::string& ctrl_msg) {
    std::istringstream iss(ctrl_msg);
    std::string token;

    RequestMsgPayload result;

    iss >> token;

    if (token != "request") {
        throw std::runtime_error("Invalid request message");
    }

    // reqNum
    if (!(iss >> result.reqNum)) {
        throw std::runtime_error("Failed to parse reqNum");
    }

    // fileName
    if (!(iss >> result.fileName)) {
        throw std::runtime_error("Failed to parse fileName");
    }

    // ttl
    if (!(iss >> result.ttl)) {
        throw std::runtime_error("Failed to parse ttl");
    }

    // remaining tokens → sourcePath
    int nodeId;
    while (iss >> nodeId) {
        result.sourcePath.push_back(nodeId);
    }

    return result;
}

std::string formatRequestCtrlMsg(const RequestMsgPayload& p) {
    std::ostringstream oss;

    oss << "request "
        << p.reqNum << " "
        << p.fileName << " "
        << p.ttl; 

        for (int id : p.sourcePath) {
            oss << " " << id;
        }

    return oss.str();
}

ReplyMsgPayload parseReplyCtrlMsg(std::string ctrl_msg) {
    std::istringstream iss(ctrl_msg);
    std::string token;

    ReplyMsgPayload result;

    // "reply"
    iss >> token;
    if (token != "reply") {
        throw std::runtime_error("Invalid message: must start with 'reply'");
    }

    // responder
    if (!(iss >> result.responder)) {
        throw std::runtime_error("Failed to parse responder");
    }

    // reqNum
    if (!(iss >> result.reqNum)) {
        throw std::runtime_error("Failed to parse reqNum");
    }

    // sourcePath
    int nodeId;
    while (iss >> nodeId) {
        result.sourcePath.push_back(nodeId);
    }

    return result;
}

std::string formatReplyCtrlMsg(const ReplyMsgPayload& p) {
    std::ostringstream oss;

    oss << "reply "
        << p.responder << " "
        << p.reqNum;

    for (int id : p.sourcePath) {
        oss << " " << id;
    }

    return oss.str();
}