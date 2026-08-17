#pragma once

class ReaderCGNSLogGuard final {
public:
    ReaderCGNSLogGuard() = default;
    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    ~ReaderCGNSLogGuard();
};
