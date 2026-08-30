#include "CgnsCore.h"
#include "ReaderAPI/ReaderApiBase.h"

#include <type_traits>

namespace ReaderAPI {
    static_assert(std::is_base_of_v<ReaderApiBase, CgnsCore>, "CgnsCore must derive from ReaderApiBase");
    static_assert(!std::is_abstract_v<CgnsCore>, "CgnsCore must implement every pure virtual function");

    extern "C" READER_API ReaderApiBase* CreateReaderCGNS()
    {
        return new CgnsCore;
    }

    extern "C" READER_API void DestroyReaderCGNS(ReaderApiBase* reader) noexcept
    {
        delete reader;
    }
} // namespace ReaderAPI
