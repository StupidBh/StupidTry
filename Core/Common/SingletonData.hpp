#pragma once
#include <memory>

#include "SingletonHolder.hpp"
#include "ThreadPool.h"

namespace stupid {
    class SingletonData final : public utils::SingletonHolder<SingletonData> {
        DELETE_COPY_AND_MOVE(SingletonData);

    public:
        std::unique_ptr<ThreadPool> m_pool;
    };
}

#define SING_DATA stupid::SingletonData::get_instance()
