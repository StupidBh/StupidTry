#pragma once
#include "SingletonHolder.hpp"

namespace stupid {
    class GlobalVarsManager final : public asst::SingletonHolder<GlobalVarsManager> {
        DELETE_COPY_AND_MOVE(GlobalVarsManager);

    public:
    };
}
