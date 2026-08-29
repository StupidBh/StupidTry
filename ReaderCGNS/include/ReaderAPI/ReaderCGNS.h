#pragma once
#include <string>

#ifdef _WIN32
    #ifdef READER_CGNS_EXPORTS
        #define READER_API __declspec(dllexport)
    #else
        #define READER_API __declspec(dllimport)
    #endif
#else
    #define READER_API __attribute__((visibility("default")))
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

        // Obtain the summary of the CGNS file, for testing purposes only
        virtual void info() = 0;

        virtual float GetVersion() const = 0;
        virtual std::string GetSolverType() const = 0;
    };

    using CreateReaderCGNSFunc = ReaderCGNS* (*)();
    using DestroyReaderCGNSFunc = void (*)(ReaderCGNS*) noexcept;
} // namespace ReaderAPI
