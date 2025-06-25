#pragma once
#include "SingletonHolder.hpp"

namespace stupid {
    class GlobalVarsManager final : public utils::SingletonHolder<GlobalVarsManager> {
        DELETE_COPY_AND_MOVE(GlobalVarsManager);

    public:
    };
}
