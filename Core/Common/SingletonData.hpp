#pragma once
#include <memory>

#include "SingletonHolder.hpp"
#include "ThreadPool.h"

namespace stupid {
    class SingletonData final : public utils::SingletonHolder<SingletonData> {
        SINGLETON_CLASS(SingletonData);
        SingletonData() = default;

    public:
        std::unique_ptr<ThreadPool> m_pool = nullptr;
    };
}

#define SINGLE_DATA stupid::SingletonData::get_instance()
