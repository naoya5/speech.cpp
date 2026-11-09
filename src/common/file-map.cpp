#include "file-map.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include "error.h"
#include "ggml.h"

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

FileMap::FileMap(const std::string & path) {
    std::unique_ptr<FILE, decltype(&std::fclose)> f(ggml_fopen(path.c_str(), "rb"), &std::fclose);
    if (!f) throw Error(Fault::Io, "cannot open " + path + "; check the path and that the file can be read");
#ifdef _WIN32
    const HANDLE file = (HANDLE) _get_osfhandle(_fileno(f.get()));
    LARGE_INTEGER length;
    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &length)) {
        throw Error(Fault::Io, "cannot read the size of " + path + ": Windows error " + std::to_string(GetLastError()));
    }
    size_ = (uint64_t) length.QuadPart;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    page_ = info.dwPageSize;
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) throw Error(Fault::Io, "cannot map " + path + " into memory: Windows error " + std::to_string(GetLastError()));
    bytes_ = (uint8_t *) MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    const DWORD error = GetLastError();
    // The view keeps the mapping open after its handle is closed.
    CloseHandle(mapping);
    if (!bytes_) throw Error(Fault::Io, "cannot map " + path + " into memory: Windows error " + std::to_string(error));
#else
    struct stat st;
    if (fstat(fileno(f.get()), &st) != 0) throw Error(Fault::Io, "cannot read the size of " + path + ": " + std::strerror(errno));
    size_ = (uint64_t) st.st_size;
    page_ = (size_t) sysconf(_SC_PAGESIZE);
    void * mapped = mmap(nullptr, (size_t) size_, PROT_READ, MAP_SHARED, fileno(f.get()), 0);
    if (mapped == MAP_FAILED) throw Error(Fault::Io, "cannot map " + path + " into memory: " + std::strerror(errno));
    bytes_ = (uint8_t *) mapped;
#endif
}

FileMap::~FileMap() {
#ifdef _WIN32
    UnmapViewOfFile(bytes_);
#else
    munmap(bytes_, (size_t) size_);
#endif
}
