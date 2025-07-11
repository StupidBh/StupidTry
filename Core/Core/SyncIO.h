#pragma once
#include "SyncController.h"

#include <vector>
#include <thread>

#include "log/logger.hpp"

class SyncIO {
    SyncController m_sync_controller;

    std::vector<int> m_buff;
    std::mutex m_buff_mtx;

public:
    void run()
    {
        m_sync_controller.init();

        std::jthread producer(&SyncIO::Read, this);
        std::jthread consumer(&SyncIO::Write, this);
    }

private:
    void Read()
    {
        int count = 1; // 用以创建测试数据

        int m = 5;     // 假设执行 5 次
        while ((m--) > 0) {
            m_sync_controller.wait_for(false);

            {
                std::lock_guard lock(m_buff_mtx);
                for (int i = 0; i < 3; ++i) {
                    m_buff.emplace_back(count++);
                }
            }

            LOG_INFO("Call Write: {}", m_buff);
            m_sync_controller.notify_one();
        }

        m_sync_controller.wait_for([this] {
            std::lock_guard lock(m_buff_mtx);
            return m_buff.empty();
        });

        m_sync_controller.stop();
        LOG_INFO("Call Write to end");
    }

    void Write()
    {
        while (!m_sync_controller.is_stopped()) {
            m_sync_controller.wait_for(true);
            if (m_sync_controller.is_stopped()) {
                LOG_DEBUG("Terminate Write after wait_for");
                break;
            }

            std::vector<int> data = {};
            {
                std::lock_guard lock(m_buff_mtx);
                data.swap(m_buff);
            }

            LOG_INFO("Call Read: {}", data.back());
            m_sync_controller.notify_one();

            // 模拟数据操作的耗时
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        LOG_INFO("Write end");
    }
};
