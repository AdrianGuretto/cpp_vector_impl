#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

// A wrapper-class for working with raw memory.
template <typename T>
class RawMemory {
public: // ------- Constructors / Destructor -------
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory& other) = delete;
    RawMemory(RawMemory&& other){
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        
        other.capacity_ = 0;
        other.buffer_ = nullptr;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

public: // ------- Methods -------

    // Exchange the values with `other` RawMemory type.
    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    // Return the pointer to the contained block of data.
    const T* GetAddress() const noexcept {
        return buffer_;
    }
    // Return the pointer to the contained block of data.
    T* GetAddress() noexcept {
        return buffer_;
    }

    // Return the capacity to store elements in the memory block.
    size_t Capacity() const {
        return capacity_;
    }


public: // ------- Operators -------

    RawMemory& operator=(const RawMemory& other) = delete;
    RawMemory& operator=(RawMemory&& other){
        if (this != &other){
            Deallocate(buffer_);
            capacity_ = 0;
            Swap(other);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

private:
    // Allocate raw memory for `n` elements and return the pointer to this memory.
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Deallocate raw memory in `buf` buffer previosly allocated.
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public: // ------- Constructors / Destructor -------

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;
    explicit Vector(size_t size) : data_(RawMemory<T>(size)), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    explicit Vector(const Vector& other) : data_(RawMemory<T>(other.Size())), size_(other.Size()) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    explicit Vector(Vector&& other){
        this->Swap(other);
    }

    ~Vector(){
        std::destroy_n(data_.GetAddress(), size_);
    }

public: // ------- Methods -------

    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept{
        return const_iterator(data_.GetAddress());
    }
    const_iterator end() const noexcept{
        return const_iterator(data_.GetAddress() + size_);
    }
    const_iterator cbegin() const noexcept{
        return const_iterator(data_.GetAddress());
    }
    const_iterator cend() const noexcept{
        return const_iterator(data_.GetAddress() + size_);
    }

    // Get size of the vector.
    size_t Size() const noexcept {
        return size_;
    }
    // Get capacity of the vector.
    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    // Reserve a specified amount of memory for the vector element type.
    void Reserve(size_t new_capacity){
        if (new_capacity <= data_.Capacity()){
            return;
        }

        RawMemory<T> new_data(new_capacity);

        __CopyMoveConstruct(data_.GetAddress(), new_data.GetAddress(), size_);

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    // Removes the last element of the vector and decremenets the size by 1.
    void PopBack() noexcept{
        if (size_ > 0){
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }

    // Changes the size of the vector to fit new_size.
    void Resize(size_t new_size){
        Reserve(new_size); // Make sure that the capacity of the vector is sufficient
        if (this->size_ > new_size){
            std::destroy_n(data_.GetAddress() + new_size, this->size_ - new_size);
        }
        else if (this->size_ < new_size){
            std::uninitialized_value_construct_n(data_.GetAddress() + this->size_, new_size - this->size_);
        }
        this->size_ = new_size;
    }

    // Adds `value` to the back of the vector.
    void PushBack(const T& value){
        EmplaceBack(std::forward<const T&>(value));
    }

    // Adds `value` to the back of the vector.
    void PushBack(T&& value){
        EmplaceBack(std::forward<T&&>(value));
    }

    // Swaps the data with `other` vector.
    void Swap(Vector& other) noexcept{
        std::swap(this->size_, other.size_);
        data_.Swap(other.data_);
    }

    // Constructs an element at the back of the the vector with `args` parameters.
    // @returns a reference to the constructed element.
    template<typename... Args>
    T& EmplaceBack(Args&&... args){
        iterator p_empl_element = nullptr;
        if (size_ == Capacity()){
            RawMemory<T> tmp_mem(size_ == 0 ? 1 : size_ * 2);
            p_empl_element = new(tmp_mem + size_) T(std::forward<Args>(args)...);

            __CopyMoveConstruct(data_.GetAddress(), tmp_mem.GetAddress(), size_);
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(tmp_mem);
        }
        else{
            p_empl_element = new(data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *p_empl_element;
    }

    // Construct an element at `pos` of the vector with `args` parameters.
    // @returns a pointer to the constructed element.
    template<typename... Args>
    iterator Emplace(const_iterator pos, Args && ...args) {
        assert(pos >= begin() && pos <= end());
        iterator p_empl_elem = nullptr;
        size_t distance = pos - begin();
        if (size_ == Capacity()) {
            RawMemory<T> tmp_data(size_ == 0 ? 1 : size_ * 2);
            p_empl_elem = new (tmp_data + distance) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), distance, tmp_data.GetAddress());
                std::uninitialized_move_n(begin() + distance, size_ - distance, tmp_data.GetAddress() + distance + 1);
            }
            else {
                try {
                    std::uninitialized_copy_n(begin(), distance, tmp_data.GetAddress());
                    std::uninitialized_copy_n(begin() + distance, size_ - distance, tmp_data.GetAddress() + distance + 1);
                }
                catch (...) {
                    std::destroy_n(tmp_data.GetAddress() + distance, 1);
                    throw;
                }
            }
            std::destroy_n(begin(), size_);
            data_.Swap(tmp_data);
        }
        else {
            if (size_ != 0) {
                new (data_ + size_) T(std::move(*(end() - 1)));
                try {
                    std::move_backward(begin() + distance, end(), end() + 1);
                }
                catch (...) {
                    std::destroy_n(end(), 1);
                    throw;
                }
                std::destroy_at(begin() + distance);
            }
            p_empl_elem = new (data_ + distance) T(std::forward<Args>(args)...);
        }
        ++size_;
        return p_empl_elem;
    }

    // Inserts `value` at the `pos` position.
    // @returns iterator to the inserted element
    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, std::forward<const T&>(value));
    }
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::forward<T&&>(value));
    }

    // Erases an element at `pos` and returns the iterator to the new element at this position.
    iterator Erase(const_iterator pos){
        assert(pos >= cbegin() && pos <= cend());
        size_t distance = pos - cbegin();
        std::move(begin() + distance + 1, end(), begin() + distance);
        PopBack();
        return begin() + distance;
    }

public: // ------- Operators -------
    // Get a value of the element under the specified `index`. 
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& other){
        size_t other_size = other.Size();
        if (this != &other){
            if (other_size > this->Capacity()){ 
                Vector other_copy(other);
                this->Swap(other_copy);
            }
            else{
                if (other_size < this->size_){
                    std::copy(other.data_.GetAddress(), other.data_.GetAddress() + other_size, this->data_.GetAddress());
                    std::destroy_n(this->data_.GetAddress() + other_size, this->size_ - other_size);
                }
                else{
                    std::copy(other.data_.GetAddress(), other.data_.GetAddress() + this->size_, this->data_.GetAddress());
                    std::uninitialized_copy_n(other.data_.GetAddress() + this->size_, other_size - this->size_, this->data_.GetAddress() + this->size_);
                }
                this->size_ = other_size;
            }
        }

        return *this;

    }
    Vector& operator=(Vector&& other){
        if (this != &other){
            this->Swap(other);
        }
        return *this;
    }
private:

    // Copies or Moves (depending on type properties) `n` number of element from `first` memory block to `result` block
    static void __CopyMoveConstruct(T* first, T* result, const size_t n){
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>){
            std::uninitialized_move_n(first, n, result);
        }
        else{
            std::uninitialized_copy_n(first, n, result);
        }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};