/** @file rotatingbuffer.hpp
 * @brief implement a rotating buffer
 */

#ifndef INCLUDED___ROTATINGBUFFER_HPP
#define INCLUDED___ROTATINGBUFFER_HPP

#include <cstddef>
#include <cassert>
#include <TinyMPU6050.h>

template<typename T = float>
struct Vector3 {
    T data[3];
    T& x() { return data[0]; }
    T& y() { return data[1]; }
    T& z() { return data[2]; }
    T x() const { return data[0]; }
    T y() const { return data[1]; }
    T z() const { return data[2]; }

    Vector3(T fst, T scn, T thrd) : data{fst, scn, thrd} {}
    Vector3() = default;
};

struct Acceleration : Vector3<> {
    using Vector3<>::Vector3;
    Acceleration() = default;
    Acceleration(Vector3<> v) : Vector3<> {v} {}
};

struct Orientation : Vector3<> {
    using Vector3<>::Vector3;
    Orientation() = default;
    Orientation(Vector3<> v) : Vector3<>{v} {}
};

struct MPU6050Data {
    Acceleration accel;
    Orientation orient;
    MPU6050Data() = default;
    MPU6050Data(const Acceleration &acc, const Orientation &ori) : accel{acc}, orient{ori} {}
    MPU6050Data(const MPU6050& mpu) :
        MPU6050Data{{mpu.GetAccX(), mpu.GetAccY(), mpu.GetAccZ()}, {mpu.GetAngX(), mpu.GetAngX(), mpu.GetAngX()}} {}
};

template<typename Type, std::ptrdiff_t Size = 100>
class RotatingBuffer {
public:
    std::ptrdiff_t index = 0;
    std::ptrdiff_t capacity = 0;

    constexpr static auto SIZE = Size;
    Type values[SIZE]{};

    void push(Type value) {
        if(capacity != SIZE) {
            values[index++] = value;
            capacity++;
        } else {
            if(index == SIZE) {
                index = 0;
            }
            values[index++] = value;
        }
    }
    Type& at(std::ptrdiff_t i) {
        assert(0 <= i && i < capacity);
        return values[(index - 1 - i) + SIZE % SIZE];
    }
    Type at(std::ptrdiff_t i) const {
        assert(0 <= i && i < capacity);
        return values[(index - 1 - i) + SIZE % SIZE];
    }

    Type& operator[](std::ptrdiff_t i) {
        return this->at(i);
    }
    Type operator[](std::ptrdiff_t i) const {
        return this->at(i);
    }
};

#endif // INCLUDED___ROTATINGBUFFER_HPP