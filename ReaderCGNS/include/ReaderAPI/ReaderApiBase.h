#pragma once
#include "io-data-type.hpp"

#include <vector>
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
    namespace Logger {
        enum LogLevel : int
        {
            READER_CGNS_LOG_TRACE = 0,
            READER_CGNS_LOG_DEBUG = 1,
            READER_CGNS_LOG_INFO = 2,
            READER_CGNS_LOG_WARN = 3,
            READER_CGNS_LOG_ERROR = 4,
            READER_CGNS_LOG_CRITICAL = 5
        };

        using LogCallback = void (*)(void* context, LogLevel level, const char* file, int line, const char* message);
    } // namespace Logger

    class ReaderApiBase {
    public:
        ReaderApiBase() = default;
        virtual ~ReaderApiBase() = default;

        ReaderApiBase(const ReaderApiBase&) = delete;
        ReaderApiBase& operator=(const ReaderApiBase&) = delete;
        ReaderApiBase(ReaderApiBase&&) = delete;
        ReaderApiBase& operator=(ReaderApiBase&&) = delete;

        virtual bool SetLogCallback(Logger::LogCallback callback, void* context) noexcept = 0;
        virtual bool ClearLogCallback() noexcept = 0;

        virtual bool Open(const std::string& cgns_file_path) = 0;
        virtual void Close() = 0;
        [[nodiscard]] virtual bool IsOpen() const = 0;

        [[nodiscard]] virtual bool GetAllElementSetName(std::vector<std::string>& element_set_names) = 0;
        [[nodiscard]] virtual bool GetAllNodeCoordinates(std::vector<Node>& node_coordinates) = 0;

        // Obtain the summary of the CGNS file, for testing purposes only
        virtual void info() const = 0;

        [[nodiscard]] virtual float GetVersion() const = 0;
        [[nodiscard]] virtual std::string GetSolverType() const = 0;
    };

    using CreateReaderCGNSFunc = ReaderApiBase* (*)();
    using DestroyReaderCGNSFunc = void (*)(ReaderApiBase*) noexcept;

} // namespace ReaderAPI
