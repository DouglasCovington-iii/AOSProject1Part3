#include <string>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <netdb.h>
#include <semaphore.h>
#include <Config.h>
#include <NetworkUtils.h>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <queue>
#include <DatabaseUtils.h>
#include <chrono>
#include <stdio.h>
#include <sstream>
#include <ProtocolParserUtils.h>
#include <LoggingUtils.h>
#include <SystemUtils.h>

extern int LOCAL_ID;
extern SharedDataStructures* sharedDataStructures;
extern Logger l;

extern sem_t fdp_init_sem;
extern sem_t recivers_init_sem;
extern std::mutex atomic_write_mtx;

u_int32_t currRecvT_id = 0;
std::mutex currRecvT_idMtx;

u_int32_t currReqNum = 0;
std::mutex reqNumMtx;

u_int32_t getNextRecvT_id() {
    std::lock_guard<std::mutex> lock(currRecvT_idMtx);
    currRecvT_id++;
    return currRecvT_id;
}

int getNextReqNum() {
    std::lock_guard<std::mutex> lock(reqNumMtx);
    currReqNum++;
    return currReqNum;
}

FiniteReqestSet::FiniteReqestSet() {
    this->fixed_capacity = 100;
    curr_size = 0;
}

FiniteReqestSet::FiniteReqestSet(u_int32_t fixed_capacity) {
    if (fixed_capacity == 0) {
        throw std::runtime_error("Fixed Capacity should be greater than 0");
    }

    this->fixed_capacity = fixed_capacity;
    curr_size = 0;
}

bool FiniteReqestSet::contains(const std::pair<int,int>& entry) {
    std::lock_guard<std::mutex> lock(mtx);
    return (seen_requests.find(entry) != seen_requests.end());
}

void FiniteReqestSet::addEntry(std::pair<int,int> entry) {
    std::lock_guard<std::mutex> lock(mtx);
    if (contains(entry)) {
        return;
    }

    if (curr_size < fixed_capacity) {
        seen_requests.insert(entry);
        arrival_ordering.push(entry);
        curr_size++;
        return;
    }

    //offload the the set a bit
    for (int i = 0; i <= fixed_capacity / 10; i++ ) {
        std::pair<int, int> e = arrival_ordering.front();
        arrival_ordering.pop();

        seen_requests.erase(e);
        curr_size--;
    }

    seen_requests.insert(entry);
    arrival_ordering.push(entry);
    curr_size++;
}

void ConnectionMultiplexer::manageConnection(int id, int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    multiplexer[id] = fd;
}

void ConnectionMultiplexer::floodAll(const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);

    for(const auto& pair : multiplexer) {
        int neighboorFd = pair.second;
        sendCMToFD(neighboorFd, controlMsg); 
    }
}

void ConnectionMultiplexer::floodAllExcept(int node, const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);

    for(const auto& pair : multiplexer) {
        if (pair.first != node) { 
            int neighboorFd = pair.second;
            sendCMToFD(neighboorFd, controlMsg); 
        }
    }
}

void ConnectionMultiplexer::sendTo(int recpientId, const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);
    int neighboorFd = multiplexer[recpientId];

    sendCMToFD(recpientId, controlMsg);
}

void BulletinBoard::postQuery(int requestNum) {
    std::lock_guard<std::mutex> lock(mtx);

    bulletinBoard[requestNum] = std::vector<int>();
}

void BulletinBoard::removeQuery(int requestNum) {
    std::lock_guard<std::mutex> lock(mtx);

    bulletinBoard.erase(requestNum);
}

std::vector<int> BulletinBoard::getResult(int requestNum) {
    std::lock_guard<std::mutex> lock(mtx);

    return bulletinBoard[requestNum];
}

void BulletinBoard::answerQueryIfApp(int responderId, int requestNum) {
    if (bulletinBoard.find(requestNum) != bulletinBoard.end()) {
        bulletinBoard[requestNum].push_back(responderId);
    }
}

int waitTimeFromHopCount(double hopCount) {
    return (hopCount / 2) + 2;
}

std::string intiateDownload(int responder_id, std::string file_name) {
    int id = LOCAL_ID;

    std::string connection_url = lookupMachineAddress(responder_id);

    addrinfo hints{};
    addrinfo* res;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(connection_url.c_str(), "5002", &hints, &res);

    if (status != 0) {
        throw std::runtime_error("Could not resolve DNS query");
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    const int MAX_TRIES = 3;
    int result = connect(sock_fd, res->ai_addr, res->ai_addrlen);
    
    int curr_try = 1; 
    while (result == -1) {
        if (curr_try >= MAX_TRIES) {
            break;
        }
        std::cout << "Node " << id << " has failed to connect to " << connection_url << " : retrying in two seconds\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "Node " << id << " is retrying to connect to " << connection_url << "\n";
        result = connect(sock_fd, res->ai_addr, res->ai_addrlen);
        curr_try++;
    }

    if (curr_try >= MAX_TRIES) {
            return "";
    }

    sendCMToFD(sock_fd, file_name);
    std::string response = recvCMFromFD(sock_fd);

    if (isError(response)) {
        return "";
    }


    std::string placeholder_dir = getDatabaseDir();
    std::string unique_file_name = file_name + "XXXXXX";

    std::string unique_file_path = placeholder_dir + "/" + unique_file_name;

    int file_fd = mkstemp(&(unique_file_path[0]));
    close(file_fd);

    unique_file_name = getLastComponentInPath(unique_file_path);
    recvFileByteStream(sock_fd, placeholder_dir, unique_file_name);

    return unique_file_path;
}

struct RequestMessage {
    int request_num;
    std::string file_name;
    int ttl;
    std::vector<int> source_path;
};

RequestMessage parseRequest(const std::string& msg) {
    std::istringstream iss(msg);

    std::string keyword;
    iss >> keyword;

    if (keyword != "request") {
        throw std::runtime_error("Invalid message type");
    }

    RequestMessage result;

    if (!(iss >> result.request_num >> result.file_name >> result.ttl)) {
        throw std::runtime_error("Malformed request header");
    }

    int node_id;
    while (iss >> node_id) {
        result.source_path.push_back(node_id);
    }

    return result;
}

void userHandlerEntry(int client_fd, int t_id) {
    /*
    const int id = LOCAL_ID;
    SharedDataStructures& sds = *sharedDataStructures

    ConnectionMultiplexer& cm = sds.connectionMultiplexer
    BullitinBoard& bb = sds.bullitinBoard
    
    string fileName = pop message from recv buffer from client_fd

    if lookUpFile(f, id)
        send (byteStreamRep f) 
        clean up
        exit
    
    int hopcount = 1
    bool foundAnswer = false;
    Vector<int> responses;

    while (hopcount <= 16) {
        int reqNum = getNextReqNum()
        int waitTime = foo hopcount 

        bb.post(reqNum)
        cm.floodAll(control message:request reqNum f hopcount id)

        wait(waitTime)
        Vector<int> responses = bb.getResults(reqNum)
        bb.remove(reqNum)
        
        if (!responses.isEmpty())
            foundAnswer = true;
            break
        
        hopcount = 2 * hopcount;
    }

    if !foundAnswer
        send err mesgage
        exit

    int idOfDealer = responses.pop();
    
    intialize socket
    create address strucut from DNS lookup of hostname of the id of the dealer
    int fd = connect on address struct

    send file_request f

    void* buff;
    
    write file buff reading f

    send bytes of file

    exit
    */

    const int id = LOCAL_ID;
    SharedDataStructures& sds = *sharedDataStructures;

    ConnectionMultiplexer& cm = sds.connectionMultiplexer;
    BulletinBoard& bb = sds.bulletinBoard;

    l.userHandlerLog(t_id, "I have spawned");

    std::string file_name = recvCMFromFD(client_fd);

    l.userHandlerLog(t_id, "I have recived a request '", file_name, "'");

    l.userHandlerLog(t_id, "Searching local database for file '", file_name, "'" );
    if (lookUpFile(file_name)) {
        std::string target_file_path = makeDbPath(file_name);

        l.userHandlerLog(t_id, "Found '", file_name, "' in local database");

        std::unique_lock<std::mutex> lock(atomic_write_mtx);
        l.userHandlerLog(t_id, "Preparing to send file located at '", target_file_path, "'");
        sendFileByteStream(client_fd, target_file_path);
        //lock.unlock();

        close(client_fd);
        return;
    }

    int request_num;
    int hop_count = 1;
    bool found_answer = false;
    std::vector<int> responses;

    while (hop_count <= 16) {
        request_num = getNextReqNum();
        double wait_time = waitTimeFromHopCount(hop_count);

        bb.postQuery(request_num);

        RequestMsgPayload request{request_num, file_name, hop_count, {id}};
        cm.floodAll(formatRequestCtrlMsg(request));

        std::this_thread::sleep_for(std::chrono::seconds((int) wait_time));
        responses = bb.getResult(request_num);
        bb.removeQuery(request_num);

        if (!responses.empty()) {
            found_answer = true;
            break;
        }

        hop_count *= 2;
    }

    if (!found_answer) {
        sendCMToFD(client_fd, "ERROR: FILE NOT FOUND");
        close(client_fd);
        return;
    }   

    sendCMToFD(client_fd, "SUCCESS");

    int idOfResponder = responses.at(0);
    std::string unique_file_path = intiateDownload(idOfResponder, file_name);

    if (unique_file_path == "") {
        sendCMToFD(client_fd, "ERROR: FILE COULD NOT BE DOWNLOADED");
        close(client_fd);
        return;
    }

    sendFileByteStream(client_fd, unique_file_path);
    close(client_fd);

    std::string db_file_path = makeDbPath(file_name);
    
    std::unique_lock<std::mutex> lock(atomic_write_mtx);
    rename(unique_file_path.c_str(), db_file_path.c_str());
    lock.unlock();

    addFileToIndex(file_name);
}

void reciverHandlerEntry(int fd, const std::string& role, int old_t_id) {
    u_int32_t t_id = getNextRecvT_id();
    l.handlerLog(Roles::RECIVER, t_id, "I have spawned from (", role, ", t_id = ", old_t_id, ")");

    sem_post(&recivers_init_sem);


    const int id = LOCAL_ID;
    SharedDataStructures& sds = *sharedDataStructures;

    ConnectionMultiplexer& cm = sds.connectionMultiplexer;
    BulletinBoard& bb = sds.bulletinBoard;
    FiniteReqestSet& request_set = sds.seen_requests;

    while (true) {
        std::string ctrl_msg = recvCMFromFD(fd);

        l.handlerLog(Roles::RECIVER, t_id, "I have recvied a control message ", ctrl_msg);

        std::string ctrl_msg_stem = getStem(ctrl_msg);

        if (ctrl_msg_stem == "request") {
            RequestMsgPayload p = parseRequstCtrlMsg(ctrl_msg);

            if (request_set.contains({p.sourcePath[0], p.reqNum})) {
                continue;
            }

            request_set.addEntry({p.sourcePath[0], p.reqNum});

            if (lookUpFile(p.fileName)) {
                int prev_node = p.sourcePath.back();

                ReplyMsgPayload newP{id, p.reqNum, p.sourcePath};
                cm.sendTo(prev_node, formatReplyCtrlMsg(newP));
                continue;
            }

            int newTTL = p.ttl - 1;

            if (newTTL > 0) {
                int prev_node = p.sourcePath.back();
                p.sourcePath.push_back(id);

                RequestMsgPayload newP{p.reqNum, p.fileName, newTTL, p.sourcePath};
                cm.floodAllExcept(prev_node, formatRequestCtrlMsg(newP));
            } 

        } else if (ctrl_msg_stem == "reply") {
            ReplyMsgPayload p = parseReplyCtrlMsg(ctrl_msg); 

            if (p.sourcePath.size() == 1 && p.sourcePath[0] == id) {
                bb.answerQueryIfApp(p.responder, p.reqNum);
            } else {
                int prev_node = p.sourcePath.back();
                p.sourcePath.pop_back();

                ReplyMsgPayload newP{p.responder, p.reqNum, p.sourcePath};
                cm.sendTo(prev_node, formatReplyCtrlMsg(newP));
            }
        }
    }
    /*
    const int id = LOCAL_ID
    SharedDataStructures& sds = *sharedDataStructures

    ConnectionMultiplexer& cm = sds.connectionMultiplexer
    BullitinBoard& bb = sds.bullitinBoard

    while (true) {
        let control_message = pop from the recv buffer or user intiated search

        let message_stem = getStem(control_message);
        switch messge_stem:
            request:
                int reqNum
                string fileName
                int ttl
                int source_id
                char* sourcePath

                if (source_id, reqNum) in seenRequests:
                    break; //assuming break applys to only switch, not out loop
                else
                    seenRequests.insert((source_id, reqNum))

                if lookUp(fileName, id)
                    let node = read last node in sourcePath (should not modify source path)
                    cm.sendTo(node, control message: reply id reqNum sourcePath)
                    break;

                
                // For all intents and purposes you shouln't flood a request to a node
                // who sent you a request but for sake of simplicity this works

                int newTTL = ttl - 1;
                if newTTL > 0
                    cm.floodAll(control message, request reqNum fileName newTTL (source_path + " " + id)
     
            reply:
                int responder;
                int reqNum;
                char* sourcePath;

                //The sourcePath should not be empty and the last node in the string should be yourself aka your local id

                if sourcePath == id:
                    bb.answerQueryIfApp(reqNum)
                else:
                    int node = pop node from end of sourcePath (should modify sourcePath)
                    cm.sendTo(node, reply responder reqNum sourcePath')
                    



            terminate_connection:
                clean up
                exit

    }
    
    */
}

void fdpHandlerEntry(int client_fd, int t_id) {
    int id = LOCAL_ID;
    std::string requested_file_name = recvCMFromFD(client_fd);

    l.fdpHandlerLog(t_id, "Recived file download request ", requested_file_name);

    if (!lookUpFile(requested_file_name)) {
        sendCMToFD(client_fd, "ERROR: Could not find " + requested_file_name + " in Node " + std::to_string(id) + " local Database");
        l.fdpHandlerLog(t_id, "ERROR: Could not find " + requested_file_name + " in Node " + std::to_string(id) + " local Database");
        return;
    }

    sendCMToFD(client_fd, "SUCCESS");

    std::string target_file_path = makeDbPath(requested_file_name);

    std::unique_lock<std::mutex> lock(atomic_write_mtx);
    sendFileByteStream(client_fd, target_file_path);
    lock.unlock();
}


void listenerEntry(int port_num, void (*handler)(int, int)) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("Failed setting socket options");
    } 

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1 ){
        throw std::runtime_error(std::string("Failed binding socket for Neighboor "));
    }

    if (listen(sock_fd, 1) == -1) {
        throw std::runtime_error("Failed to listen on socket");
    }

    l.listenerLog(port_num, "I have been spawned");

    //if handler is fdp, aquire
    if (handler == fdpHandlerEntry) {
        l.log("fdp sempahore got signaled");
        sem_post(&fdp_init_sem);
    }
    // should siginal that that reciever can start

    int t_id = 1;
    while (true) {
        int client_fd = accept(sock_fd, nullptr, nullptr);

        if (client_fd == -1) {
            throw std::runtime_error("Failed to accept connection on welcoming socket");
        }

        l.listenerLog(port_num, "Accepted a connection");

        // std::cout << "\n-------------------------------------------------------------\n";
        // std::cout << "Node " << id << " recieved a connection";
        // std::cout << "\n-------------------------------------------------------------\n";

        std::thread t(handler, client_fd, t_id);
        t.detach();

        l.listenerLog(port_num, "Spawned its ", t_id, " thread");
        t_id++;
    }
}

// void reciverListenerEntry(sem_t& sem) {
//     int id = LOCAL_ID;
//     SharedDataStructures& sds = *sharedDataStructures;
//     ConnectionMultiplexer& connectionMultiplexer = sds.connectionMultiplexer;



//     int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
//     int opt = 1;

//     if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
//         throw std::runtime_error("Failed setting socket options");
//     } 

//     sockaddr_in addr{};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(5001);
//     addr.sin_addr.s_addr = INADDR_ANY;

//     if (bind(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1 ){
//         throw std::runtime_error(std::string("Failed binding socket for Neighboor "));
//     }

//     if (listen(sock_fd, 1) == -1) {
//         throw std::runtime_error("Failed to listen on socket");
//     }

//     while (true) {
//         int client_fd = accept(sock_fd, nullptr, nullptr);

//         if (client_fd == -1) {
//             throw std::runtime_error("Failed to accept connection on welcoming socket");
//         }

//         std::cout << "\n-------------------------------------------------------------\n";
//         std::cout << "Node " << id << " recieved a connection";
//         std::cout << "\n-------------------------------------------------------------\n";

//         //put client_fd
//         sem_post(&sem);

//         std::thread t(reciverHandlerEntry, client_fd);
//         t.detach();
//     }

//     //now you can send and recive using send and recv wrt to the unistd.h using client_fd
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

void recpientEntry(int fd, int t_id) {
    SharedDataStructures& sds = *sharedDataStructures;
    ConnectionMultiplexer& cm = sds.connectionMultiplexer;

    l.recipientHandlerLog(t_id, "I have been spawned");

    l.recipientHandlerLog(t_id, "Asking for connection neighboor id");
    std::string msg = recvCMFromFD(fd);
    l.recipientHandlerLog(t_id, "Connection responsing saying its node ", msg);

    int recipentId = std::stoi(msg);
    cm.manageConnection(recipentId, fd);

    l.recipientHandlerLog(t_id, "Initialized connection ", recipentId);
    reciverHandlerEntry(fd, Roles::RECIPIENT, t_id);
}

void intiatorEntry(int neighboor_id, int t_id) {
    int id = LOCAL_ID;
    SharedDataStructures& sds = *sharedDataStructures;
    ConnectionMultiplexer& cm = sds.connectionMultiplexer;

    l.initiatorHandlerLog(t_id, "I have been spawned");

    //not nessary
    // if (id >= neighboor_id) {
    //     throw std::invalid_argument("Breaking total order");
    // }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd == -1) {
        std::cerr << "socket failed\n";
        std::exit(1);
    }

    l.initiatorHandlerLog(t_id, "Looking up node ", neighboor_id, " hostname");
    std::string connection_url = lookupMachineAddress(neighboor_id);
    l.initiatorHandlerLog(t_id, "Resolved node ", neighboor_id, " as ", connection_url);

    addrinfo hints{};
    addrinfo* res;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(connection_url.c_str(), "5001", &hints, &res);

    if (status != 0) {
        throw std::runtime_error("Could not resolve DNS query");
    }

    l.initiatorHandlerLog(t_id, "Node ", id, " is trying to connect to ", connection_url);

    int rc = connect(sock_fd, res->ai_addr, res->ai_addrlen);

    while (rc == -1) {

        l.initiatorHandlerLog(t_id, "Node ", id, " has failed to connect to ", connection_url, " : retrying in two seconds");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        l.initiatorHandlerLog(t_id, "Node ", id, " is retrying to connect to ", connection_url);

        rc = connect(sock_fd, res->ai_addr, res->ai_addrlen);
    }

    
    l.initiatorHandlerLog(t_id, "Node ", id, " connected to ", connection_url);
  

    //sem_post(&sem);

    l.initiatorHandlerLog(t_id, "Sending Neighboor ", neighboor_id, " my id");
    sendCMToFD(sock_fd, std::to_string(id));

    cm.manageConnection(neighboor_id, sock_fd);

    l.initiatorHandlerLog(t_id, "Intilized connection for neighboor ", neighboor_id);
    reciverHandlerEntry(sock_fd, Roles::INTIATOR, t_id);

    //can send and recv using unistd.h using sock_fd

    //current thread waits for connection from neighboor
}
