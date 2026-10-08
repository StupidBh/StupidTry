#pragma once
#include "Types/ElementTypes.hpp"

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

    class FluidExtensionsBase {
    public:
        FluidExtensionsBase() = default;
        virtual ~FluidExtensionsBase() = default;

        FluidExtensionsBase(const FluidExtensionsBase&) = delete;
        FluidExtensionsBase& operator=(const FluidExtensionsBase&) = delete;
        FluidExtensionsBase(FluidExtensionsBase&&) = delete;
        FluidExtensionsBase& operator=(FluidExtensionsBase&&) = delete;

        // Requires an open file; returns false if velocity metadata is unavailable or unreadable.
        // Queries share the owning reader's thread-safety constraints.
        [[nodiscard]] virtual bool HasVelocityField() const = 0;
    };

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

        [[nodiscard]] virtual bool GetAllNodeCoordinates(std::vector<Node>& node_coordinates) = 0;
        [[nodiscard]] virtual bool GetAllElement(std::vector<Elem>& elements) = 0;
        [[nodiscard]] virtual bool GetAllElement(ElementTable& elements) = 0;

        [[nodiscard]] virtual bool GetAllComponentName(std::vector<std::string>& component_names) = 0;
        [[nodiscard]] virtual bool GetComponent(const std::string& name, std::vector<Integer>& ids) = 0;

        [[nodiscard]] virtual bool GetAllFieldFunctionName(std::vector<Field>& field_names) = 0;
        [[nodiscard]] virtual int GetFieldFunctionPosition(const std::string& var, const std::string& sub_var) = 0;
        [[nodiscard]] virtual bool GetFieldFunctionIds(const std::string& var, const std::string& sub_var, std::vector<Integer>& ids) = 0;
        [[nodiscard]] virtual bool GetFieldFunctionData(const std::string& var, const std::string& sub_var, std::vector<Real>& data) = 0;

        // Obtain the summary of the CGNS file, for testing purposes only
        virtual void info() const = 0;

        // Optional capability; nullptr means this reader does not provide fluid extensions.
        // A non-null pointer is borrowed from the reader and must not be deleted.
        // It remains valid until reader destruction; Close()/Open() changes the queried file.
        // Stop extension calls before destroying the reader or unloading its module.
        virtual FluidExtensionsBase* GetFluidExtensions() { return nullptr; }

        [[nodiscard]] virtual Real GetVersion() const = 0;
        [[nodiscard]] virtual std::string GetSolverType() const = 0;
    };

    using CreateReaderCGNSFunc = ReaderApiBase* (*)();
    using DestroyReaderCGNSFunc = void (*)(ReaderApiBase*) noexcept;

} // namespace ReaderAPI
