#ifndef PPU_H
#define PPU_H
#include <string>
#include <vector>
#include <stdexcept>

struct RequestMsgPayload {
    int reqNum;
    std::string fileName;
    int ttl;
    std::vector<int> sourcePath;
};

struct ReplyMsgPayload {
    int responder;
    int reqNum;
    std::vector<int> sourcePath;
};

bool isError(std::string cntrl_msg);
std::string getLastComponentInPath(std::string path);
std::string getStem(std::string cntrl_mesg);

RequestMsgPayload parseRequstCtrlMsg(const std::string& ctrl_msg);
std::string formatRequestCtrlMsg(const RequestMsgPayload& p);

ReplyMsgPayload parseReplyCtrlMsg(std::string ctrl_msg);
std::string formatReplyCtrlMsg(const ReplyMsgPayload& p);
#endif