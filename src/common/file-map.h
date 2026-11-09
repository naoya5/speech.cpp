#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * A file mapped read-only. Its pages belong to the system's file cache and not to the process: macOS counts none of
 * them in the process's memory footprint, and under memory pressure drops them and reads them again from the file
 * instead of writing them to swap. A file that cannot be opened, measured or mapped throws an Error of Io.
 */
class FileMap {
public:
    explicit FileMap(const std::string & path);
    ~FileMap();
    FileMap(const FileMap &) = delete;
    FileMap & operator=(const FileMap &) = delete;

    /** The file's first byte. The pages are read-only, though ggml takes them as writable. */
    uint8_t * bytes() const { return bytes_; }
    uint64_t size() const { return size_; }
    /** The system's page size, on which a buffer over the map starts. */
    size_t page() const { return page_; }

private:
    uint8_t * bytes_ = nullptr;
    uint64_t size_ = 0;
    size_t page_ = 0;
};
