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

bool FiniteReqestSet::internalContains(const std::pair<int,int>& entry) {
    bool contains = (seen_requests.find(entry) != seen_requests.end());
    l.fsLog("Entry (", entry.first, ", ", entry.second, ") *", (contains ? "is": "is not"), "* contained in FS");

    return (contains);
}

bool FiniteReqestSet::contains(const std::pair<int,int>& entry) {
    std::lock_guard<std::mutex> lock(mtx);
    bool contains = (seen_requests.find(entry) != seen_requests.end());
    l.fsLog("Entry (", entry.first, ", ", entry.second, ") *", (contains ? "is": "is not"), "* contained in FS");

    return (contains);
}

void FiniteReqestSet::addEntry(std::pair<int,int> entry) {
    std::lock_guard<std::mutex> lock(mtx);

    l.fsLog("Preparing to add entry, curr_size=", curr_size, ", fixed_capacity=", fixed_capacity);
    
    if (internalContains(entry)) {
        l.fsLog("Entry (", entry.first, ", ", entry.second, ") already contained in FS");
        return;
    }

    if (curr_size < fixed_capacity) {
        l.fsLog("Enough space to add entry (", entry.first, ", ", entry.second, "), preparing to add entry");
        seen_requests.insert(entry);
        arrival_ordering.push(entry);

        curr_size++;
        l.fsLog("Successfully added entry (", entry.first, ", ", entry.second, ")");
        return;
    }

    l.fsLog("Finite Set has reached its capacity, offloading ", (fixed_capacity / 10) + 1);

    //offload the the set by a tenth, (i.e 100 FC would kick out 11)
    //Starting a 0 is a defensive mechaninism if the integer division is 0
    for (int i = 0; i <= fixed_capacity / 10; i++ ) {
        std::pair<int, int> e = arrival_ordering.front();
        l.fsLog("Preparing to remove entry (", e.first, ", ", e.second, ")");
        arrival_ordering.pop();

        seen_requests.erase(e);
        curr_size--;

        l.fsLog("Successfully removed entry (", e.first, ", ", e.second, "), curr_size=", curr_size);
    }

    l.fsLog("Successfully clear room, curr_size=", curr_size);
    l.fsLog("Preparing to insert entry (", entry.first, ", ", entry.second, ")");

    seen_requests.insert(entry);
    arrival_ordering.push(entry);
    curr_size++;

    l.fsLog("Successfully inserted entry (", entry.first, ", ", entry.second, ")");
}

void ConnectionMultiplexer::manageConnection(int id, int fd) {
    std::lock_guard<std::mutex> lock(mtx);

    l.cmLog("Added Connection (", id, ", ", fd, ") to CM");
    multiplexer[id] = fd;
}

void ConnectionMultiplexer::floodAll(const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);

    l.cmLog("Preparing to flood control message: '", controlMsg, "'");

    for(const auto& pair : multiplexer) {
        int neighboorFd = pair.second;

        l.cmLog("Preparing to send to Node ", pair.first, " through fd ", pair.second);
        sendCMToFD(neighboorFd, controlMsg); 
    }

    l.cmLog("Flooded control message: '", controlMsg, "'");
}

void ConnectionMultiplexer::floodAllExcept(int node, const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);

    l.cmLog("Preparing to flood control message: '", controlMsg, "' except to Node ", node);
    for(const auto& pair : multiplexer) {
        if (pair.first != node) { 
            l.cmLog("Preparing to send to Node ", pair.first, " through fd ", pair.second);

            int neighboorFd = pair.second;
            sendCMToFD(neighboorFd, controlMsg); 
        }
    }

    l.cmLog("Flooded control message: '", controlMsg, "' except to Node ", node);
}

void ConnectionMultiplexer::sendTo(int recpientId, const std::string& controlMsg) {
    std::lock_guard<std::mutex> lock(mtx);
    int neighboorFd = multiplexer[recpientId];

    l.cmLog("Preparing to send control_msg='", controlMsg, "', through (", recpientId, ", ", neighboorFd,")");
    sendCMToFD(neighboorFd, controlMsg);
}

void BulletinBoard::postQuery(int req_num) {
    std::lock_guard<std::mutex> lock(mtx);

    bulletinBoard[req_num] = std::vector<int>();
    l.bbLog("Posted ", req_num, " with [] to the BB");
}

void BulletinBoard::removeQuery(int req_num) {
    std::lock_guard<std::mutex> lock(mtx);

    bulletinBoard.erase(req_num);
    l.bbLog("Removed ", req_num, " from the BB");
}

std::vector<int> BulletinBoard::getResult(int req_num) {
    std::lock_guard<std::mutex> lock(mtx);

    std::vector<int> result = bulletinBoard[req_num];
    std::ostringstream oss;

    oss << "[";
    for(int i = 0; i < result.size(); i++) {
        oss << i << (i != (result.size() - 1)? " ":"]");
    }

    l.bbLog("Retrived list of results from req_num=", req_num,  ": ",  oss.str());
    return result;
}

void BulletinBoard::answerQueryIfApp(int responderId, int requestNum) {
    if (bulletinBoard.find(requestNum) != bulletinBoard.end()) {
        bulletinBoard[requestNum].push_back(responderId);

        l.bbLog("Placed resp_id=", responderId, " in req_num=", requestNum, " bucket");
        return;
    }

    l.bbLog("Didn't find a entry for req_num=", requestNum);
}

int waitTimeFromHopCount(double hopCount) {
    return (hopCount / 2) + 2;
}


//This really should be in system utils but I would have
//to refactor DB utils to not use shared memory 
std::string intiateDownload(int responder_id, std::string file_name) {
    int id = LOCAL_ID;

    l.systemLog("Preparing to intiate Download for Machine ", responder_id, " for file '", file_name, "'");
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    std::string connection_url = lookupMachineAddress(responder_id);
    addrinfo res = dnsLookup(connection_url, 5002);

    bool connection_failed;
    bool have_another_attempt = false;
    int attempts_tried = 0;
    const int MAX_TRIES = 3;
    int wait_time_per_attempts = 2; // seconds

    do {
        l.systemLog("Attempt ", (attempts_tried + 1), " to connect to '", connection_url, "'");
        int rc = connect(sock_fd, res.ai_addr, res.ai_addrlen);
        attempts_tried++;

        //Connection set up was succfell
        connection_failed = rc == -1;

        if (!connection_failed) {
            break;
        }

        l.systemLog("Node ", id, " has failed to connect to '", connection_url, "'");
        have_another_attempt = (attempts_tried < MAX_TRIES);

        if (have_another_attempt) {
            l.systemLog("Node ", id, " will retry to connect to '", connection_url, "' in ", wait_time_per_attempts, " seconds");
            std::this_thread::sleep_for(std::chrono::seconds(wait_time_per_attempts));
        }

    } while (connection_failed && have_another_attempt);

    if (connection_failed) {
        l.systemLog("Failed to connect have after ", MAX_TRIES, " attempts");
        return ""; // indicator to caller that could not dowload file;
    }

    sendCMToFD(sock_fd, file_name);
    std::string response = recvCMFromFD(sock_fd);

    if (response != "SUCCESS") {
        l.systemLog("Node ", responder_id,  " response: ", response);
        return ""; // indicator to caller that could not dowload file;
    }

    std::string placeholder_dir = getDatabaseDir();
    std::string unique_file_name = file_name + "XXXXXX"; //template for mkstemp

    std::string unique_file_path = placeholder_dir + "/" + unique_file_name;

    l.systemLog("Preparing to make unqiue temporary file with a templated_path='", unique_file_path, "'");
    int file_fd = mkstemp(&(unique_file_path[0]));
    l.systemLog("Created unqiue temporary file located '", unique_file_path, "'");

    close(file_fd);

    unique_file_name = getLastComponentInPath(unique_file_path);

    l.systemLog("Preparing to download ", file_name, " in ", placeholder_dir, " named ", unique_file_name);
    recvFileByteStream(sock_fd, placeholder_dir, unique_file_name);

    return unique_file_path;
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

        sendCMToFD(client_fd, "SUCCESS");
        sendFileByteStream(client_fd, target_file_path);
        //lock.unlock();

        close(client_fd);
        return;
    }

    int MAX_HOP_COUNT = 16;
    int request_num;
    int hop_count = 1;
    bool found_answer = false;
    std::vector<int> responses;

    l.userHandlerLog(t_id, "Did not find '", file_name, "' in local database");

    l.userHandlerLog(t_id, "Commencing search amongst peers");
    while (hop_count <= MAX_HOP_COUNT) {
        l.userHandlerLog(t_id, "Trying with max_hopcount=", hop_count);

        l.userHandlerLog(t_id, "Requesting unique request_num");
        request_num = getNextReqNum();
        l.userHandlerLog(t_id, "Generated unique request_num=", request_num);

        l.userHandlerLog(t_id, "Creating post for request_num=", request_num, " on BB");
        bb.postQuery(request_num);

        RequestMsgPayload request{request_num, file_name, hop_count, {id}};

        l.userHandlerLog(t_id, "Sending Control message: ", request);
        cm.floodAll(formatRequestCtrlMsg(request));

        double wait_time = waitTimeFromHopCount(hop_count);

        l.userHandlerLog(t_id, "Preparing to wait ", wait_time, " seconds");
        std::this_thread::sleep_for(std::chrono::seconds((int) wait_time));
        l.userHandlerLog(t_id, "FINISHED WAITING ", wait_time, " SECONDS");

        responses = bb.getResult(request_num);
        l.userHandlerLog(t_id, "Recvied request set from req_num=", request_num, ": ", formatNeighboorList(responses));

        bb.removeQuery(request_num);
        l.userHandlerLog(t_id, "Removed req_num=", request_num, "from BB");

        if (!responses.empty()) {
            l.userHandlerLog(t_id, "Found a responder");

            found_answer = true;
            break;
        }

        l.userHandlerLog(t_id, "Did not find a file within ", hop_count);
        hop_count *= 2;
    }

    if (!found_answer) {
        l.userHandlerLog(t_id, "Did not find file within a maximum hop count of ", MAX_HOP_COUNT);
        sendCMToFD(client_fd, "ERROR: FILE NOT FOUND");
        close(client_fd);
        return;
    }   

    int idOfResponder = responses.at(0);

    l.userHandlerLog(t_id, "Preparing to download ", file_name, " from machine ", idOfResponder);
    std::string unique_file_path = intiateDownload(idOfResponder, file_name);

    if (unique_file_path == "") {
        sendCMToFD(client_fd, "ERROR: FILE COULD NOT BE DOWNLOADED");
        close(client_fd);
        return;
    }

    l.userHandlerLog(t_id, "Sending success return code");
    sendCMToFD(client_fd, "SUCCESS");

    l.userHandlerLog(t_id, "Sending downloaded file");

    sendFileByteStream(client_fd, unique_file_path);
    close(client_fd); //The user Handler job is dones
    l.userHandlerLog(t_id, "Closed Connection");

    std::string db_file_path = makeDbPath(file_name);
    
    std::unique_lock<std::mutex> lock(atomic_write_mtx);

    l.userHandlerLog(t_id, "Adding/(Potentially overwritting) saving copy to database as ", db_file_path);
    rename(unique_file_path.c_str(), db_file_path.c_str());
    lock.unlock();

    l.userHandlerLog(t_id, "Adding file ", db_file_path, " to local index");
    addFileToIndex(file_name);
    l.userHandlerLog(t_id, "OPERATION FINISHED, TERMINATING EXECUTION");
}

void reciverHandlerEntry(int fd, const std::string& role, int old_t_id) {
    u_int32_t t_id = getNextRecvT_id();
    l.reciverHandlerLog(t_id, "I have spawned from (", role, ", t_id = ", old_t_id, ")");

    //Signal to driver that one connection has been made
    sem_post(&recivers_init_sem);

    const int id = LOCAL_ID;
    SharedDataStructures& sds = *sharedDataStructures;

    ConnectionMultiplexer& cm = sds.connectionMultiplexer;
    BulletinBoard& bb = sds.bulletinBoard;
    FiniteReqestSet& request_set = sds.seen_requests;

    while (true) {
        l.reciverHandlerLog(t_id, "Waiting for control message");
        std::string ctrl_msg = recvCMFromFD(fd);
        std::string ctrl_msg_stem = getStem(ctrl_msg);

        l.reciverHandlerLog(t_id, "I have recvied a control message '", ctrl_msg, "' with ctrl_msg_stm=", ctrl_msg_stem);

        if (ctrl_msg_stem == "request") {
            RequestMsgPayload p = parseRequstCtrlMsg(ctrl_msg);

            l.reciverHandlerLog(t_id, "Parsed_req_msg=", p);

            int source_orginator = p.sourcePath[0];

            if (request_set.contains({source_orginator, p.reqNum})) {
                l.reciverHandlerLog(t_id, "Ignoring seened request from '", source_orginator, "' with a req_num=", p.reqNum);
                continue;
            }

            l.reciverHandlerLog(t_id, "Adding '", source_orginator, "' with a req_num=", p.reqNum, " to seen requests");
            
            // Implicity contructing a std::pair<int,int> object
            request_set.addEntry({source_orginator, p.reqNum});

            l.reciverHandlerLog(t_id, "Looking up to see if '", p.fileName, "' is in local database");
            if (lookUpFile(p.fileName)) {
                l.reciverHandlerLog(t_id, "Found '", p.fileName, "' in local database, preparing to send reply");
                int prev_node = p.sourcePath.back();
                ReplyMsgPayload newP{id, p.reqNum, p.sourcePath};

                l.reciverHandlerLog(t_id, "Sending to prev_node=", prev_node, " reply_payload=", newP);
                cm.sendTo(prev_node, formatReplyCtrlMsg(newP));

                continue;
            }

            l.reciverHandlerLog(t_id, "Did not find '", p.fileName, "' in local database");
            int newTTL = p.ttl - 1;

            if (newTTL > 0) {
                int prev_node = p.sourcePath.back();
                p.sourcePath.push_back(id);
                RequestMsgPayload newP{p.reqNum, p.fileName, newTTL, p.sourcePath};

                l.reciverHandlerLog(t_id, "Flooding new reply message except to '", prev_node ,"', req_msg_payload=", newP);
                cm.floodAllExcept(prev_node, formatRequestCtrlMsg(newP));

                continue;
            }
            
            l.reciverHandlerLog(t_id, "TTL=", newTTL, ", DROPPING REQUEST");

        } else if (ctrl_msg_stem == "reply") {
            ReplyMsgPayload p = parseReplyCtrlMsg(ctrl_msg);
            
            l.reciverHandlerLog(t_id, "parsed_reply_payload=", p);

            if (p.sourcePath.size() == 1 && p.sourcePath[0] == id) {
                l.reciverHandlerLog(t_id, "Found a reply for me, preparing to post on BB req_num=", p.reqNum, ", resp_id=", p.responder);
                bb.answerQueryIfApp(p.responder, p.reqNum);
            } else {
                int prev_node = p.sourcePath.back();
                p.sourcePath.pop_back();
                ReplyMsgPayload newP{p.responder, p.reqNum, p.sourcePath};
                l.reciverHandlerLog(t_id, "Redirecting Reply back to source, sending to ", prev_node, ", ctrl_msg='", formatReplyCtrlMsg(newP), "'");
                cm.sendTo(prev_node, formatReplyCtrlMsg(newP));
            }
        }

        l.reciverHandlerLog(t_id, "Recived unknown ctrl_msg with stem='", ctrl_msg_stem,"'");
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
    l.fdpHandlerLog(t_id, "I have spawned");
    int id = LOCAL_ID;

    l.fdpHandlerLog(t_id, "Listening for request");
    std::string requested_file_name = recvCMFromFD(client_fd);
    l.fdpHandlerLog(t_id, "Recived file download request ", requested_file_name);

    if (!lookUpFile(requested_file_name)) {
        l.fdpHandlerLog(t_id, "ERROR: Could not find " + requested_file_name + " in Node " + std::to_string(id) + " local Database");
        sendCMToFD(client_fd, "ERROR: Could not find " + requested_file_name + " in Node " + std::to_string(id) + " local Database");

        close(client_fd);
        return;
    }

    sendCMToFD(client_fd, "SUCCESS");

    std::string target_file_path = makeDbPath(requested_file_name);

    std::unique_lock<std::mutex> lock(atomic_write_mtx);
    l.fdpHandlerLog(t_id, "Preparing to send '", requested_file_name, "' located='", target_file_path, "'");
    sendFileByteStream(client_fd, target_file_path);
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
