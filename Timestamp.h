#pragma once
#include <iostream>
#include <cstdint>
#include <string>
class Timestamp {
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch); // explicit 防止隐式转换
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};