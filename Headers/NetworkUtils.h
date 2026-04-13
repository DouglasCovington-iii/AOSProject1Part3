#ifndef NETWORK_H
#define NETWORK_H
#include <string>
#include <semaphore.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>

class FiniteReqestSet {
    private:
        struct PairHash {
                size_t operator()(const std::pair<int,int>& p) const {
                    return p.first * 31 + p.second;
                }
            };
        
        bool internalContains(const std::pair<int,int>& entry);
        std::mutex mtx;
        u_int32_t fixed_capacity;
        u_int32_t curr_size;
        std::queue<std::pair<int, int>> arrival_ordering;
        std::unordered_set<std::pair<int, int>, PairHash> seen_requests;
    public:
            FiniteReqestSet();
            FiniteReqestSet(u_int32_t fixed_capacity);

            bool contains(const std::pair<int,int>& entry);
            void addEntry(std::pair<int,int> entry);
};

class ConnectionMultiplexer {
    private:
        std::unordered_map<int, int> multiplexer;
        std::mutex mtx;
    public:
        void manageConnection(int id, int fd);
        void floodAll(const std::string& msg);
        void floodAllExcept(int node, const std::string& msg);
        void sendTo(int recpient_id, const std::string& msg);
};

class BulletinBoard {
    private:
        std::unordered_map<int, std::vector<int>> bulletinBoard;
        std::mutex mtx;
    public:
        void postQuery(int requestNum);
        void removeQuery(int requestNum);
        std::vector<int> getResult(int requestNum);

        void answerQueryIfApp(int Id, int requestNum);
};

struct SharedDataStructures {
    ConnectionMultiplexer connectionMultiplexer;
    BulletinBoard bulletinBoard;
    FiniteReqestSet seen_requests;
};

std::string lookupMachineAddress(int id);

void listenerEntry(int portNum, void (*handler)(int, int));

void reciverHandlerEntry(int client_fd, const std::string& role, int old_t_id);
void fdpHandlerEntry(int client_fd, int t_id);
void userHandlerEntry(int client_fd, int t_id);

void recpientEntry(int client_fd, int t_id);
void intiatorEntry(int neighboor_id, int t_id);


#endif