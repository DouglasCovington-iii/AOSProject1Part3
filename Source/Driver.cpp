#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <stdexcept>
#include <MatrixUtils.h>
#include <NetworkUtils.h>
#include <DatabaseUtils.h>
#include <semaphore.h>
#include <mutex>
#include <LoggingUtils.h>
#include <Config.h>

//DON'T WRITE INTO THIS, can't make it const because it's assigned at runntime
int LOCAL_ID = -1;
SharedDataStructures* sharedDataStructures;
Logger l;

sem_t fdp_init_sem;
sem_t recivers_init_sem;

std::mutex atomic_write_mtx;
std::mutex file_mtx;
std::mutex atomic_print;

int main(int argc, char** argv) {

    const char* filename = "topology1.txt";

    SharedDataStructures sds;
    sharedDataStructures = &sds;

    sem_init(&fdp_init_sem, 0, 0);
    sem_init(&recivers_init_sem, 0, 0);

    if (argc != 2) {
        throw std::invalid_argument("Invalid number of arguments: driver.out [id]");
    }
    
    int id = std::stoi(argv[1]);
    LOCAL_ID = id;

    //std::string target_dir = config::LOG_DIR + "/" + std::to_string(id) + "-Log";
    //l.setUpFile(target_dir); 

    l.logDelimiter();

    l.log("My ID is ", id);
    std::vector<int> neighboors;

    std::string created_dir = initLocalDatabase();
    l.log("Created Local Database at ", created_dir);
    
    Matrix<bool> topology1 = loadTMatrix(filename, "M1");
    int n = topology1.numOfCols();

    l.log("Number of Nodes: ", n);
   
    l.logWithSpacing(1, 1, "Topology:");
    printMatrix(topology1);

    Matrix<bool> result = topology1;
    Matrix<bool> acc = topology1;

    for(int i = 2; i <= n; i++) {
        acc = booleanProduct(acc, topology1);
        result = matrixUnion(result, acc);
    }

    l.logWithSpacing(1, 1, "Transtive closure of topology graph:");
    printMatrix(result);

    for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
            if (!result.get(i, j)) {
                l.logAndThrowError("The graph is not connected");
            }
        }
    }

    l.logWithSpacing(1, 0, "graph is connected");
    
    //load your neigboors
    int id_index = id - 1;
    for(int i = 0; i < n; i++) {
        if (topology1.get(id_index, i)) {
            neighboors.push_back(i + 1);
        }
    }
    
    l.logDelimiter(1, 1);

    l.logWithSpacing(0, 1, "Node ", id, " will connect with it neighboors: ");
    std::string neighboor_str;
    
    neighboor_str = "[";
    for(int i = 0; i < neighboors.size(); i++) {
        neighboor_str += std::to_string(neighboors[i]) + ( (i != neighboors.size() -1) ? " ": "]");
    }

    l.log(neighboor_str);

    l.logDelimiter(1, 0);
    // l.handlerLog(Roles::DRIVER, 1, "Hit there friend", " yes", " sir ", false, " true");
    // l.handlerLog(Roles::INTIATOR, 4, "I'm ...", 1);




    // // Set up your ability to share files
    std::thread fdpListener(listenerEntry, 5002, fdpHandlerEntry);
    // l.log("main thread is waiting");
    sem_wait(&fdp_init_sem);
    l.log("main thread past fdp semaphore");
    std::thread reciverListener(listenerEntry, 5001, recpientEntry );

    //It's not nessary to have the FDP listener set up assuming error handling is set up for failing to connect to the FDP listner

//     // Allow yourself to join the distrubuted file system
//     std::thread reciverListener(listenerEntry, 5001, recpientEntry );

    int t_id = 1;
    for (int neighboor : neighboors) {
        if (id < neighboor) {
            std::thread t(intiatorEntry, neighboor, t_id);
            t.detach();

            t_id++;
        }
    }

//     //wait for all connections to be established by the adjency matrix
    for (int i = 1; i <= neighboors.size(); i++) {
        sem_wait(&recivers_init_sem);

        l.log("Have ", i, " / ", neighboors.size(), " connections");
    }

//     l.importantLog("Node ", id," has connected to all its neighboors");

//     /*
//     Open the door to your customers
//     */
   
    std::thread userListener(listenerEntry, 5000, userHandlerEntry);
    userListener.join();

    fdpListener.join();
    reciverListener.join();
}
