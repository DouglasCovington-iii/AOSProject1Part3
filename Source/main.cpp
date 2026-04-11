#include <LoggingUtils.h>
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


int main() {
    Logger l;

    l.setUpFile("Logs/This is my file");

    l.log("check ", "this ", 2, " shit ", "out");
}