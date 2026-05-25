#include "small_string.h"
#include <iostream>
#include <cassert>

void test_default_constructor() {
    SmallString s;
    assert(s.empty());
    assert(s.size() == 0);
    assert(std::string(s.c_str()) == "");
    std::cout << "✓ test_default_constructor passed\n";
}

void test_from_cstring_short() {
    SmallString s("hello");
    assert(s.size() == 5);
    assert(std::string(s.c_str()) == "hello");
    std::cout << "✓ test_from_cstring_short passed\n";
}

void test_from_cstring_long() {
    const char* long_str = "this is a very long string that exceeds SSO capacity";
    SmallString s(long_str);
    assert(s.size() == std::strlen(long_str));
    assert(std::string(s.c_str()) == long_str);
    std::cout << "✓ test_from_cstring_long passed\n";
}

void test_copy_constructor() {
    SmallString s1("hello world");
    SmallString s2(s1);
    assert(s1 == s2);
    s1.push_back('!');
    assert(s1 != s2);
    std::cout << "✓ test_copy_constructor passed\n";
}

void test_move_constructor() {
    SmallString s1("hello world");
    SmallString s2(std::move(s1));
    assert(std::string(s2.c_str()) == "hello world");
    assert(s1.empty());
    std::cout << "✓ test_move_constructor passed\n";
}

void test_push_back() {
    SmallString s;
    s.push_back('a');
    s.push_back('b');
    assert(s.size() == 2);
    assert(std::string(s.c_str()) == "ab");
    std::cout << "✓ test_push_back passed\n";
}

void test_append() {
    SmallString s("hello");
    s.append(" world");
    assert(std::string(s.c_str()) == "hello world");
    std::cout << "✓ test_append passed\n";
}

void test_grow_from_sso_to_large() {
    SmallString s("short");
    for (int i = 0; i < 30; ++i) {
        s.push_back('x');
    }
    assert(s.size() == 5 + 30);
    assert(std::strlen(s.c_str()) == s.size());
    std::cout << "✓ test_grow_from_sso_to_large passed\n";
}

void test_clear() {
    SmallString s("something");
    s.clear();
    assert(s.empty());
    assert(std::string(s.c_str()) == "");
    std::cout << "✓ test_clear passed\n";
}

void test_subscript() {
    SmallString s("test");
    assert(s[0] == 't');
    s[0] = 'T';
    assert(s[0] == 'T');
    std::cout << "✓ test_subscript passed\n";
}

void test_comparison() {
    SmallString a("abc");
    SmallString b("abc");
    SmallString c("abd");
    assert(a == b);
    assert(a != c);
    assert(a < c);
    std::cout << "✓ test_comparison passed\n";
}

int main() {
    std::cout << "Running tests for SmallString...\n\n";
    
    test_default_constructor();
    test_from_cstring_short();
    test_from_cstring_long();
    test_copy_constructor();
    test_move_constructor();
    test_push_back();
    test_append();
    test_grow_from_sso_to_large();
    test_clear();
    test_subscript();
    test_comparison();
    
    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
