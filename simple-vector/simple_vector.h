#pragma once

#include <cassert>
#include <initializer_list>
#include <algorithm>
#include <utility>
#include <iterator>
#include <stdexcept>
#include "array_ptr.h"

struct ReserveProxyObj {
    ReserveProxyObj(size_t capacity) : capacity_(capacity){}
    size_t capacity_;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template <typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;
    
    explicit SimpleVector(ReserveProxyObj reserve) : capacity_(reserve.capacity_), data_(ArrayPtr<Type>(reserve.capacity_)) {}

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size) : SimpleVector(size, Type {}) {}

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, const Type& value) : size_(size), capacity_(size), data_(ArrayPtr<Type>(size))  { 
        std::fill(begin(), end(), value);
    }

    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init) : SimpleVector(init.size()) {
        std::copy(init.begin(), init.end(), begin());
    }
    
    SimpleVector(const SimpleVector& other) {
        SimpleVector temp(other.size_);
        std::copy(other.begin(), other.end(), temp.begin());
        this->swap(temp);
    }
    
    SimpleVector(SimpleVector&& other) : data_(other.data_.Release()) {
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    
    SimpleVector& operator=(const SimpleVector& rhs) {
        if (this != &rhs) {
            SimpleVector temp(rhs);
            this->swap(temp);
        }
        return *this;
    }

    // Возвращает количество элементов в массиве
    size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Выход за пределы массива");
        }
        return *(data_.Get() + index);
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Выход за пределы массива");
        }
        return *(data_.Get() + index);
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    // Изменяет размер массива.
    // При увеличении размера новые элементы получают значение по умолчанию для типа Type
    void Resize(size_t new_size) {
        if (new_size <= size_) {
            size_ = new_size;
            return;
        }
        if (new_size <= capacity_) {
            std::generate(end(), end() + new_size - size_, [](){return Type {};});
        } else {
            size_t new_capacity = std::max(new_size, capacity_ * 2);
            IncreaseCapacity(new_capacity);
        }
        size_ = new_size;
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            IncreaseCapacity(new_capacity);
        }
    }
    
    // Добавляет элемент в конец вектора
    // При нехватке места увеличивает вдвое вместимость вектора
    void PushBack(const Type& item) {
        if (size_ >= capacity_) {
            size_t new_capacity = std::max(size_ + 1, capacity_ * 2);
            IncreaseCapacity(new_capacity);
        }
        *(data_.Get() + size_) = item;
        ++size_;
    }
    
    void PushBack(Type&& item) {
        if (size_ >= capacity_) {
            size_t new_capacity = std::max(size_ + 1, capacity_ * 2);
            IncreaseCapacity(new_capacity);
        }
        *(data_.Get() + size_) = std::move(item);
        ++size_;
    }
    
    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
    }
    
    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, const Type& value) {
    	assert(pos >= begin() && pos <= end());
        size_t index = pos - begin();
        if (size_ < capacity_) {
            std::copy_backward(std::make_move_iterator(begin() + index), std::make_move_iterator(end()), end() + 1);
            data_[index] = value;
        } else {
            size_t new_capacity = std::max(size_ + 1, capacity_ * 2);
            ArrayPtr<Type> new_data(new_capacity);
            std::copy(std::make_move_iterator(begin()), std::make_move_iterator(begin() + index), new_data.Get());
            new_data[index] = value;
            std::copy(std::make_move_iterator(begin() + index), std::make_move_iterator(end()), new_data.Get() + index + 1);
            new_data.swap(data_);
            capacity_ = new_capacity;
        }
        ++size_;
        return &data_[index];
    }
    
    Iterator Insert(ConstIterator pos, Type&& value) {
    	assert(pos >= begin() && pos <= end());
        size_t index = pos - begin();
        if (size_ < capacity_) {
            std::copy_backward(std::make_move_iterator(begin() + index), std::make_move_iterator(end()), end() + 1);
            data_[index] = std::move(value);
        } else {
            size_t new_capacity = std::max(size_ + 1, capacity_ * 2);
            ArrayPtr<Type> new_data(new_capacity);
            std::copy(std::make_move_iterator(begin()), std::make_move_iterator(begin() + index), new_data.Get());
            new_data[index] = std::move(value);
            std::copy(std::make_move_iterator(begin() + index), std::make_move_iterator(end()), new_data.Get() + index + 1);
            new_data.swap(data_);
            capacity_ = new_capacity;
        }
        ++size_;
        return &data_[index];
    }
    
    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) {
    	assert(pos >= begin() && pos < end());
        size_t index = pos - begin();
        std::copy(std::make_move_iterator(begin() + index + 1), std::make_move_iterator(end()), begin() + index);
        --size_;
        return &data_[index];
    }
    
    void swap(SimpleVector& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        data_.swap(other.data_);
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        return data_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        return data_.Get() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return cbegin();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return cend();
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return data_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return data_.Get() + size_;
    }
private:
    size_t size_ = 0;
    size_t capacity_ = 0;
    ArrayPtr<Type> data_;
    
    void IncreaseCapacity(size_t new_capacity) {
        ArrayPtr<Type> new_data(new_capacity);
        std::copy(std::make_move_iterator(begin()), std::make_move_iterator(end()), new_data.Get());
        new_data.swap(data_);
        capacity_ = new_capacity;
    }
};

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    if (lhs.GetSize() != rhs.GetSize()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(rhs < lhs);
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return rhs < lhs;
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs);
} 
