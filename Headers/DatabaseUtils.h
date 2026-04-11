#ifndef DATABASE_UTILS_H
#define DATABASE_UTILS_H

#include <string>

std::string initLocalDatabase();
bool lookUpFile(const std::string& file_nameame);
void addFileToIndex(std::string file_name);

std::string getDatabaseDir();
std::string makeDbPath(std::string file_name);

#endif