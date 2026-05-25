#pragma once

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <stdexcept>

class SmallString {
public:
    static constexpr size_t SSO_CAPACITY = 22;

    SmallString();
    explicit SmallString(const char* str);
    SmallString(const SmallString& other);
    SmallString(SmallString&& other) noexcept;
    ~SmallString();

    SmallString& operator=(const SmallString& other);
    SmallString& operator=(SmallString&& other) noexcept;

    void push_back(char c);
    void append(const char* str);
    void append(const SmallString& other);
    void clear();

    const char* c_str() const;
    const char* data() const;
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    char& operator[](size_t pos) { return data_[pos]; }
    const char& operator[](size_t pos) const { return data_[pos]; }

    char* begin() { return data_; }
    char* end() { return data_ + size_; }
    const char* begin() const { return data_; }
    const char* end() const { return data_ + size_; }

    bool operator==(const SmallString& other) const;
    bool operator!=(const SmallString& other) const;
    bool operator<(const SmallString& other) const;

private:
    char* data_;
    size_t size_;
    size_t capacity_;
    char sso_buffer_[SSO_CAPACITY + 1];
    bool is_sso_;

    void grow(size_t new_cap);
    void copy_from(const char* src, size_t len);
    void free_memory();
    void set_sso(const char* str, size_t len);
    void set_large(char* ptr, size_t len, size_t cap);
};
