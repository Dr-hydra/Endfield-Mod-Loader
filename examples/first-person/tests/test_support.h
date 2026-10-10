#pragma once
#include <cstdlib>
#include <iostream>
inline int checks = 0;
#define CHECK(value) do { ++checks; if(!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; std::exit(1); } } while(false)
inline bool nearly(float a, float b) { return std::abs(a-b)<0.0001f; }
