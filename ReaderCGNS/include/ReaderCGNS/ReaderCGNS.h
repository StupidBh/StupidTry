#pragma once
#include <string>

#ifdef _WIN32
    #ifdef READERCGNS_BUILD
        #define READER_CGNS_DLL __declspec(dllexport)
    #else
        #define READER_CGNS_DLL __declspec(dllimport)
    #endif
#else
    #define READER_CGNS_DLL
#endif

namespace ReaderAPI {
    class ReaderCGNS {
    public:
        ReaderCGNS() = default;
        virtual ~ReaderCGNS() = default;

        ReaderCGNS(const ReaderCGNS&) = delete;
        ReaderCGNS& operator=(const ReaderCGNS&) = delete;
        ReaderCGNS(ReaderCGNS&&) = delete;
        ReaderCGNS& operator=(ReaderCGNS&&) = delete;

        virtual bool Open(const std::string& cgns_file_path) = 0;
        virtual void Close() = 0;
        virtual bool IsOpen() const = 0;

        virtual bool info() = 0;

        virtual void* QueryInterface() = 0;
    };

    using CreateReaderCGNSFunc = ReaderCGNS* (*)();
    using DestroyReaderCGNSFunc = void (*)(ReaderCGNS*) noexcept;
} // namespace ReaderAPI
