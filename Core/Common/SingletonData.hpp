#pragma once
#include "SingletonHolder.hpp"

namespace stupid {
    class SingletonHolder final : public utils::SingletonHolder<SingletonHolder> {
        DELETE_COPY_AND_MOVE(SingletonHolder);

    public:
    };
}

#define GV_DATA stupid::SingletonHolder::get_instance()
