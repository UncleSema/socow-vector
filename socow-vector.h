#pragma once
#include <array>
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
  using iterator = T*;
  using const_iterator = T const*;

  socow_vector() {}

  socow_vector(socow_vector const& other)
      : size_(other.size_), small_(other.small_) {
    if (small_) {
      copy(other.begin(), other.end(), begin());
    } else {
      new (&buffer_) buffer(other.buffer_);
    }
  }

  socow_vector& operator=(socow_vector const& other) {
    if (this == &other) {
      return *this;
    }
    socow_vector temp = socow_vector(other);
    temp.swap(*this);
    return *this;
  }

  ~socow_vector() {
    if (small_ || buffer_.unique()) {
      destroy_elements(begin(), end());
    }
    if (!small_) {
      buffer_.~buffer();
    }
    size_ = 0;
  }

  T& operator[](size_t i) {
    assert(size_ > i);
    return data()[i];
  }

  T const& operator[](size_t i) const {
    assert(size_ > i);
    return cdata()[i];
  }

  T* data() {
    if (small_) {
      return static_buffer_.data();
    }
    unshare();
    return buffer_.data();
  }

  T const* data() const {
    return cdata();
  }

  T const* cdata() const {
    return (small_ ? static_buffer_.data() : buffer_.data());
  }

  const_iterator cbegin() const {
    return cdata();
  }

  const_iterator cend() const {
    return cdata() + size_;
  }

  size_t size() const {
    return size_;
  }

  T& front() {
    assert(size_ > 0);
    return data()[0];
  }

  T const& front() const {
    assert(size_ > 0);
    return cdata()[0];
  }

  T& back() {
    assert(size_ > 0);
    return data()[size_ - 1];
  }

  T const& back() const {
    assert(size_ > 0);
    return cdata()[size_ - 1];
  }

  void push_back(T const& e) {
    if (size_ != capacity()) {
      new (end()) T(e);
    } else {
      buffer new_buffer(2 * capacity());
      copy(cbegin(), cend(), new_buffer.data());
      try {
        new (new_buffer.data() + size_) T(e);
      } catch (...) {
        new_buffer.~buffer();
        throw;
      }
      if (!small_) {
        destroy_buffer(*this);
      } else {
        destroy_elements(begin(), end());
      }
      new (&buffer_) buffer(new_buffer);
      small_ = false;
    }
    ++size_;
  }

  void pop_back() {
    unshare();
    size_--;
    cend()->~T();
  }

  bool empty() const {
    return size() == 0;
  }

  size_t capacity() const {
    return (small_ ? SMALL_SIZE : buffer_.capacity());
  }

  void reserve(size_t new_cap) {
    if (small_ && new_cap > SMALL_SIZE) {
      make_big(new_cap);
    } else if (!small_ && new_cap >= size_ && !buffer_.unique()) {
      new (&buffer_) buffer(realloc(new_cap, cbegin(), cend()));
    }
  }

  void shrink_to_fit() {
    if (!small_) {
      if (size_ <= SMALL_SIZE) {
        buffer temp = buffer_;
        buffer_.~buffer();
        try {
          copy(temp.data(), temp.data() + size_, static_buffer_.begin());
        } catch (...) {
          new (&buffer_) buffer(temp);
          throw;
        }
        if (temp.unique()) {
          destroy_elements(temp.data(), temp.data() + size_);
        }
        small_ = true;
      } else if (size_ != buffer_.capacity()) {
        new (&buffer_) buffer(realloc(size_, cbegin(), cend()));
      }
    }
  }

  void clear() {
    if (small_ || buffer_.unique()) {
      destroy_elements(begin(), end());
    } else {
      size_t cap = buffer_.capacity();
      destroy_buffer(*this);
      new (&buffer_) buffer(cap);
    }
    size_ = 0;
  }

  void swap(socow_vector& other) {
    using std::swap;
    if (small_ && other.small_) {
      for (size_t i = 0; i < std::min(size_, other.size_); i++) {
        swap(static_buffer_[i], other.static_buffer_[i]);
      }
      if (size_ < other.size_) {
        copy(other.static_buffer_.begin() + size_,
             other.static_buffer_.begin() + other.size_,
             static_buffer_.begin() + size_);
        destroy_elements(other.static_buffer_.begin() + size_,
                         other.static_buffer_.begin() + other.size_);
      } else {
        copy(static_buffer_.begin() + other.size_,
             static_buffer_.begin() + size_,
             other.static_buffer_.begin() + other.size_);
        destroy_elements(static_buffer_.begin() + other.size_,
                         static_buffer_.begin() + size_);
      }
    } else if (!small_ && !other.small_) {
      swap(other.buffer_, buffer_);
    } else if (small_ && !other.small_) {
      swap_small_big(*this, other);
    } else {
      swap_small_big(other, *this);
    }
    swap(other.size_, size_);
    swap(small_, other.small_);
  }

  iterator begin() {
    return data();
  }

  iterator end() {
    return data() + size_;
  }

  const_iterator begin() const {
    return cbegin();
  }

  const_iterator end() const {
    return cend();
  }

  iterator insert(const_iterator pos, T const& value) {
    size_t index = pos - cbegin();
    push_back(value);
    iterator iter = begin() + index;
    using std::swap;
    for (iterator i = end() - 1; i != iter; i--) {
      swap(*i, *(i - 1));
    }
    return iter;
  }

  iterator erase(const_iterator pos) {
    return erase(pos, pos + 1);
  }

  iterator erase(const_iterator first, const_iterator last) {
    size_t index1 = first - cbegin();
    size_t index2 = last - cbegin();
    size_t to = cend() - last;
    using std::swap;
    for (size_t i = 0; i < to; i++) {
      swap(data()[i + index1], data()[i + index2]);
    }
    for (int i = 0; i < index2 - index1; i++) {
      pop_back();
    }
    return begin() + index1;
  }

private:
  struct buffer {
    buffer() : buffer_data_(nullptr) {}

    explicit buffer(size_t capacity)
        : buffer_data_(static_cast<buffer_data*>(operator new(
              sizeof(buffer_data) + sizeof(T) * capacity))) {
      buffer_data_->links = 1;
      buffer_data_->capacity_ = capacity;
    }

    buffer(buffer const& other) : buffer_data_(other.buffer_data_) {
      buffer_data_->links++;
    }

    buffer& operator=(buffer const& other) {
      if (&other != this) {
        buffer temp(other);
        using std::swap;
        swap(temp.buffer_data_, this->buffer_data_);
      }
      return *this;
    }

    ~buffer() {
      if (unique()) {
        operator delete(buffer_data_);
      } else {
        buffer_data_->links--;
      }
    }

    size_t capacity() const {
      return buffer_data_->capacity_;
    }

    T* data() {
      return buffer_data_->data_;
    }

    T* data() const {
      return buffer_data_->data_;
    }

    bool unique() {
      return buffer_data_->links == 1;
    }

  private:
    struct buffer_data {
      size_t links;
      size_t capacity_;
      T data_[0];
    };

    buffer_data* buffer_data_;
  };

  void make_big(size_t new_cap) {
    buffer new_buffer = realloc(new_cap, cbegin(), cend());
    destroy_elements(begin(), end());
    new (&buffer_) buffer(new_buffer);
    small_ = false;
  }

  void unshare() {
    if (!small_ && !buffer_.unique()) {
      new (&buffer_) buffer(realloc(buffer_.capacity(), cbegin(), cend()));
    }
  }

  void copy(const_iterator begin, const_iterator end, iterator dest) {
    for (const_iterator it = begin; it < end; it++) {
      try {
        new (dest + (it - begin)) T(*it);
      } catch (...) {
        destroy_elements(dest, dest + (it - begin));
        throw;
      }
    }
  }

  buffer realloc(size_t new_capacity, const_iterator begin,
                 const_iterator end) {
    if (new_capacity == 0) {
      small_ = true;
      return buffer();
    }
    size_ = end - begin;
    buffer new_buffer(new_capacity);
    copy(begin, end, new_buffer.data());
    if (!small_) {
      destroy_buffer(*this);
    }
    return new_buffer;
  }

  static void destroy_elements(iterator begin, iterator end) {
    for (auto it = begin; it != end; it++) {
      it->~T();
    }
  }

  static void destroy_buffer(socow_vector& vector) {
    if (!vector.small_) {
      if (vector.buffer_.unique()) {
        destroy_elements(vector.begin(), vector.end());
      }
      vector.buffer_.~buffer();
    }
  }

  void swap_small_big(socow_vector& small, socow_vector& big) {
    buffer temp = big.buffer_;
    big.buffer_.~buffer();
    try {
      copy(small.static_buffer_.begin(),
           small.static_buffer_.begin() + small.size_,
           big.static_buffer_.begin());
    } catch (...) {
      new (&big.buffer_) buffer(temp);
      throw;
    }
    destroy_elements(small.begin(), small.end());
    new (&small.buffer_) buffer(temp);
  }

  size_t size_{0};
  bool small_{true};
  union {
    std::array<T, SMALL_SIZE> static_buffer_;
    buffer buffer_;
  };
};