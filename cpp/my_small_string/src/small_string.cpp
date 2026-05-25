#include "small_string.h"
#include "pool_allocator.h"
#include <cstring>
#include <algorithm>

SmallString::SmallString()
    : size_(0), capacity_(SSO_CAPACITY), is_sso_(true) {
    data_ = sso_buffer_;
    sso_buffer_[0] = '\0';
}

SmallString::SmallString(const char* str) {
    size_t len = std::strlen(str);
    if (len <= SSO_CAPACITY) {
        set_sso(str, len);
    } else {
        char* ptr = static_cast<char*>(PoolAllocator::instance().allocate(len + 1));
        std::memcpy(ptr, str, len + 1);
        set_large(ptr, len, len);
    }
}

SmallString::SmallString(const SmallString& other) {
    if (other.is_sso_) {
        set_sso(other.data_, other.size_);
    } else {
        char* ptr = static_cast<char*>(PoolAllocator::instance().allocate(other.capacity_ + 1));
        std::memcpy(ptr, other.data_, other.size_ + 1);
        set_large(ptr, other.size_, other.capacity_);
    }
}

SmallString::SmallString(SmallString&& other) noexcept
    : data_(other.data_),
      size_(other.size_),
      capacity_(other.capacity_),
      is_sso_(other.is_sso_) {
    if (other.is_sso_) {
        std::memcpy(sso_buffer_, other.sso_buffer_, SSO_CAPACITY + 1);
        data_ = sso_buffer_;
        other.data_ = other.sso_buffer_;
        other.sso_buffer_[0] = '\0';
        other.size_ = 0;
    } else {
        other.data_ = other.sso_buffer_;
        other.is_sso_ = true;
        other.size_ = 0;
        other.capacity_ = SSO_CAPACITY;
        other.sso_buffer_[0] = '\0';
    }
}

SmallString::~SmallString() {
    free_memory();
}

SmallString& SmallString::operator=(const SmallString& other) {
    if (this != &other) {
        free_memory();
        if (other.is_sso_) {
            set_sso(other.data_, other.size_);
        } else {
            char* ptr = static_cast<char*>(PoolAllocator::instance().allocate(other.capacity_ + 1));
            std::memcpy(ptr, other.data_, other.size_ + 1);
            set_large(ptr, other.size_, other.capacity_);
        }
    }
    return *this;
}

SmallString& SmallString::operator=(SmallString&& other) noexcept {
    if (this != &other) {
        free_memory();
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        is_sso_ = other.is_sso_;
        if (other.is_sso_) {
            std::memcpy(sso_buffer_, other.sso_buffer_, SSO_CAPACITY + 1);
            data_ = sso_buffer_;
            other.data_ = other.sso_buffer_;
            other.sso_buffer_[0] = '\0';
            other.size_ = 0;
        } else {
            other.data_ = other.sso_buffer_;
            other.is_sso_ = true;
            other.size_ = 0;
            other.capacity_ = SSO_CAPACITY;
            other.sso_buffer_[0] = '\0';
        }
    }
    return *this;
}

void SmallString::push_back(char c) {
    if (size_ == capacity_) {
        grow(capacity_ == 0 ? 1 : capacity_ * 2);
    }
    data_[size_++] = c;
    data_[size_] = '\0';
}

void SmallString::append(const char* str) {
    size_t len = std::strlen(str);
    if (size_ + len > capacity_) {
        grow(size_ + len);
    }
    std::memcpy(data_ + size_, str, len + 1);
    size_ += len;
}

void SmallString::append(const SmallString& other) {
    append(other.data_);
}

void SmallString::clear() {
    free_memory();
    set_sso("", 0);
}

const char* SmallString::c_str() const {
    return data_;
}

const char* SmallString::data() const {
    return data_;
}

bool SmallString::operator==(const SmallString& other) const {
    return size_ == other.size_ && std::memcmp(data_, other.data_, size_) == 0;
}

bool SmallString::operator!=(const SmallString& other) const {
    return !(*this == other);
}

bool SmallString::operator<(const SmallString& other) const {
    int cmp = std::memcmp(data_, other.data_, std::min(size_, other.size_));
    if (cmp != 0) return cmp < 0;
    return size_ < other.size_;
}

// private helpers
void SmallString::grow(size_t new_cap) {
    if (new_cap <= SSO_CAPACITY) {
        // switch to SSO if we were large
        if (!is_sso_) {
            char tmp[SSO_CAPACITY + 1];
            std::memcpy(tmp, data_, size_);
            tmp[size_] = '\0';
            free_memory();
            set_sso(tmp, size_);
        } else {
            // already SSO, but new_cap <= SSO_CAPACITY -> just update capacity?
            // Actually if we are SSO, capacity_ == SSO_CAPACITY, so nothing to do.
        }
    } else {
        // need large allocation
        size_t alloc_cap = new_cap;
        char* new_data = static_cast<char*>(PoolAllocator::instance().allocate(alloc_cap + 1));
        std::memcpy(new_data, data_, size_);
        new_data[size_] = '\0';
        free_memory();
        set_large(new_data, size_, alloc_cap);
    }
}

void SmallString::copy_from(const char* src, size_t len) {
    if (len <= SSO_CAPACITY) {
        set_sso(src, len);
    } else {
        char* ptr = static_cast<char*>(PoolAllocator::instance().allocate(len + 1));
        std::memcpy(ptr, src, len + 1);
        set_large(ptr, len, len);
    }
}

void SmallString::free_memory() {
    if (!is_sso_ && data_) {
        PoolAllocator::instance().deallocate(data_, capacity_ + 1);
    }
    data_ = nullptr;
    size_ = 0;
    capacity_ = 0;
}

void SmallString::set_sso(const char* str, size_t len) {
    is_sso_ = true;
    capacity_ = SSO_CAPACITY;
    size_ = len;
    data_ = sso_buffer_;
    std::memcpy(sso_buffer_, str, len);
    sso_buffer_[len] = '\0';
}

void SmallString::set_large(char* ptr, size_t len, size_t cap) {
    is_sso_ = false;
    data_ = ptr;
    size_ = len;
    capacity_ = cap;
}
