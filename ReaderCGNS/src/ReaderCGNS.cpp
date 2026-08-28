#include "CgnsCore.h"

#include <type_traits>

namespace ReaderAPI {
    static_assert(std::is_base_of_v<ReaderCGNS, CgnsCore>, "CgnsCore must derive from ReaderCGNSBase");
    static_assert(!std::is_abstract_v<CgnsCore>, "CgnsCore must implement every pure virtual function");

    extern "C" READER_CGNS_DLL ReaderCGNS* CreateReaderCGNS()
    {
        return new CgnsCore;
    }

    extern "C" READER_CGNS_DLL void DestroyReaderCGNS(ReaderCGNS* reader) noexcept
    {
        delete reader;
    }
} // namespace ReaderAPI
