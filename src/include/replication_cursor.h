#ifndef DRIVER_REPLICATION_CURSOR_H
#define DRIVER_REPLICATION_CURSOR_H
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cerrno>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <cctype>
inline std::string replication_cursor_name(const std::string &identity) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : identity) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return "replication-" + std::to_string(hash) + ".cursor";
}
inline bool read_replication_cursor(const std::string &path, const std::string &identity,
                                    uint64_t &cursor) {
    std::ifstream file(path);
    if (!file) {
        if (access(path.c_str(), F_OK) == 0)
            throw std::runtime_error("cannot read replication checkpoint");
        return false;
    }
    std::string stored;
    std::getline(file, stored);
    std::string number;
    if (stored != identity || !(file >> number) || number.empty())
        throw std::runtime_error("invalid replication checkpoint");
    for (unsigned char c : number)
        if (!std::isdigit(c))
            throw std::runtime_error("invalid replication cursor number");
    cursor = std::stoull(number);
    return true;
}
inline void write_replication_cursor(const std::string &path, const std::string &identity,
                                     uint64_t cursor) {
    std::string temporary = path + ".tmp.XXXXXX";
    std::vector<char> temporary_name(temporary.begin(), temporary.end());
    temporary_name.push_back(0);
    const auto content = identity + "\n" + std::to_string(cursor) + "\n";
    int fd = mkstemp(temporary_name.data());
    temporary = temporary_name.data();
    if (fd < 0)
        throw std::runtime_error("cannot create replication checkpoint");
    size_t offset = 0;
    while (offset < content.size()) {
        const auto written = write(fd, content.data() + offset, content.size() - offset);
        if (written <= 0) {
            close(fd);
            unlink(temporary.c_str());
            throw std::runtime_error("cannot write replication checkpoint");
        }
        offset += written;
    }
    const int synced = fsync(fd);
    const int closed = close(fd);
    if (synced || closed || rename(temporary.c_str(), path.c_str())) {
        unlink(temporary.c_str());
        throw std::runtime_error("cannot commit replication checkpoint");
    }
}
#endif
