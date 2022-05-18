#pragma once
#include <cstddef>
#include <array>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
  using iterator = T*;
  using const_iterator = T const*;

  socow_vector();
  socow_vector(socow_vector const&);
  socow_vector& operator=(socow_vector const& other);

  ~socow_vector();

  T& operator[](size_t i);
  T const& operator[](size_t i) const;

  T* data();
  T const* data() const;
  T const* cdata() const;
  size_t size() const;

  T& front();
  T const& front() const;

  T& back();
  T const& back() const;
  void push_back(T const&);
  void pop_back();

  bool empty() const;

  size_t capacity() const;
  void reserve(size_t);
  void shrink_to_fit();
  void clear();

  void swap(socow_vector&);

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  const_iterator cbegin() const;
  const_iterator cend() const;

  iterator insert(const_iterator pos, T const&);

  iterator erase(const_iterator pos);

  iterator erase(const_iterator first, const_iterator last);

private:
  struct buffer {
    size_t links_;
    T* data_;

    buffer(T* data, size_t links) : data_(data), links_(links) {}
  };

  union {
    std::array<T, SMALL_SIZE> static_buffer_;
    buffer* buffer_;
  };

  size_t size_{0};
  bool small_{true};
  size_t capacity_{SMALL_SIZE};

  T* copy(size_t new_cap, socow_vector<T, SMALL_SIZE> const& socow_vector);

  T* copy(T* new_data, size_t new_cap,
          socow_vector<T, SMALL_SIZE> const& socow_vector);
  void resize(size_t new_cap);
  void destroy_data(buffer* buffer, size_t len);
  void destroy_elements(T* data, size_t len);
  void destroy();
  void unshare();
  void make_big(size_t new_cap);
  void make_small();
};

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector() = default;

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector(
    socow_vector<T, SMALL_SIZE> const& other)
    : small_(other.small_), capacity_(other.capacity_), size_(other.size_) {
  if (small_) {
    static_buffer_ = other.static_buffer_;
  } else {
    buffer_ = other.buffer_;
    buffer_->links_++;
  }
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>& socow_vector<T, SMALL_SIZE>::operator=(
    socow_vector<T, SMALL_SIZE> const& other) {
  if (&other == this)
    return *this;
  socow_vector<T, SMALL_SIZE> copy_socow_vector(other);
  swap(copy_socow_vector);
  return *this;
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::~socow_vector() {
  destroy();
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::operator[](size_t i) {
  assert(size_ > i);
  return data()[i];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::operator[](size_t i) const {
  assert(size_ > i);
  return cdata()[i];
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::data() {
  if (small_) {
    return static_buffer_.data();
  }
  unshare();
  return buffer_->data_;
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::data() const {
  return cdata();
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::cdata() const {
  return (small_ ? static_buffer_.data() : buffer_->data_);
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::size() const {
  return size_;
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::front() {
  assert(size_ > 0);
  return data()[0];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::front() const {
  assert(size_ > 0);
  return cdata()[0];
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::back() {
  assert(size_ > 0);
  return data()[size_ - 1];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::back() const {
  assert(size_ > 0);
  return cdata()[size_ - 1];
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::push_back(T const& value) {
  if (size_ != capacity_) {
    new (begin() + size_++) T(value);
    return;
  }
  size_t new_cap = 2 * size_ + (size_ == 0);
  T* data = copy(new_cap, *this);
  new (data + size_) T(value);
  destroy();
  buffer_ = new buffer(data, 1);
  size_++;
  small_ = false;
  capacity_ = new_cap;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::pop_back() {
  assert(size_ > 0);
  back().~T();
  size_--;
}

template <typename T, size_t SMALL_SIZE>
bool socow_vector<T, SMALL_SIZE>::empty() const {
  return size_ == 0;
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::capacity() const {
  return capacity_;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::reserve(size_t new_cap) {
  unshare();
  if (new_cap > capacity_) {
    resize(new_cap);
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::shrink_to_fit() {
  if (capacity_ > size_) {
    resize(size_);
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::clear() {
  destroy_elements(begin(), size_);
  size_ = 0;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::swap(socow_vector<T, SMALL_SIZE>& other) {
  if (small_) {
    make_big(SMALL_SIZE);
  }
  if (other.small_) {
    other.make_big(SMALL_SIZE);
  }
  std::swap(other.buffer_, buffer_);
  std::swap(other.size_, size_);
  std::swap(other.small_, small_);
  std::swap(other.capacity_, capacity_);
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::begin() {
  return data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::end() {
  return data() + size_;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::begin() const {
  return cbegin();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::end() const {
  return cend();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::cbegin() const {
  return cdata();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::cend() const {
  return cdata() + size_;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::insert(const_iterator pos, T const& value) {
  size_t index = pos - cbegin();
  push_back(value);
  iterator iter = begin() + index;
  for (iterator i = end() - 1; i != iter; i--) {
    std::swap(*i, *(i - 1));
  }
  return iter;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(const_iterator pos) {
  return erase(pos, pos + 1);
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(const_iterator first, const_iterator last) {
  size_t index1 = first - cbegin();
  size_t index2 = last - cbegin();
  size_t to = cend() - last;
  unshare();
  for (size_t i = 0; i < to; i++) {
    std::swap(data()[i + index1], data()[i + index2]);
  }
  for (int i = 0; i < index2 - index1; i++) {
    pop_back();
  }
  return begin() + index1;
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::copy(size_t new_cap,
                                     socow_vector const& socow_vector) {
  T* new_data = static_cast<T*>(operator new(sizeof(T) * new_cap));
  try {
    return copy(new_data, new_cap, socow_vector);
  } catch (...) {
    operator delete(new_data);
    throw;
  }
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::copy(T* new_data, size_t new_cap,
                                     socow_vector const& socow_vector) {
  if (new_cap == 0) {
    return new_data;
  }
  size_t copied = 0;
  try {
    for (const_iterator i = socow_vector.begin(); i != socow_vector.end();
         copied++, i++) {
      new (copied + new_data) T(*i);
    }
  } catch (...) {
    destroy_elements(new_data, copied);
    throw;
  }
  return new_data;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::resize(size_t new_cap) {
  if (new_cap <= SMALL_SIZE) {
    if (!small_) {
      make_small();
    }
    return;
  }
  if (small_) {
    make_big(new_cap);
    return;
  }
  unshare();
  T* new_data = copy(new_cap, *this);
  destroy();
  buffer_ = new buffer(new_data, 1);
  capacity_ = new_cap;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::destroy_data(buffer* buffer, size_t len) {
  if (buffer->links_ == 1) {
    destroy_elements(buffer->data_, len);
    operator delete(buffer->data_);
    delete buffer;
  } else {
    buffer_->links_--;
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::destroy_elements(T* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    data[i].~T();
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::destroy() {
  if (small_) {
    destroy_elements(static_buffer_.data(), size_);
  } else {
    destroy_data(buffer_, size_);
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::unshare() {
  if (!small_) {
    if (buffer_->links_ == 1)
      return;

    try {
      --buffer_->links_;
      auto* new_data = copy(capacity_, *this);
      buffer_ = new buffer(new_data, 1);
    } catch (...) {
      ++buffer_->links_;
      throw;
    }
  }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::make_big(size_t new_cap) {
  assert(small_);
  auto* new_data = copy(new_cap, *this);
  destroy();
  buffer_ = new buffer(new_data, 1);
  capacity_ = new_cap;
  small_ = false;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::make_small() {
  assert(!small_);
  unshare();
  size_t new_size = std::min(size_, SMALL_SIZE);
  T* temp = copy(new_size, *this);
  destroy();
  size_ = new_size;
  for (size_t i = 0; i < size_; i++) {
    new (i + static_buffer_.begin()) T(temp[i]);
  }
  destroy_elements(temp, size_);
  operator delete(temp);
  capacity_ = SMALL_SIZE;
  small_ = true;
}
