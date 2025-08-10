#pragma once
#include <memory>

#include "SingletonHolder.hpp"
#include "ThreadPool.h"

#include "boost/program_options.hpp"

namespace stupid {
    inline static constexpr const char* APP_NAME = "stupid-app";
    inline static constexpr const char* APP_VERSION = "0.1.0";

    class SingletonData final : public utils::SingletonHolder<SingletonData> {
        SINGLETON_CLASS(SingletonData);
        SingletonData() = default;

    public:
        std::unique_ptr<ThreadPool> m_pool;
        boost::program_options::variables_map m_vm;
    };
}

#define SINGLE_DATA stupid::SingletonData::get_instance()
