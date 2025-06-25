#pragma once
#include "SingletonHolder.hpp"

namespace stupid {
    class SingletonData final : public utils::SingletonHolder<SingletonData> {
        DELETE_COPY_AND_MOVE(SingletonData);

    public:
    };
}

#define GV_DATA stupid::SingletonData::get_instance()
