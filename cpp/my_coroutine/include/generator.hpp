#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include <functional>
#include <optional>
#include <ucontext.h>
#include "pool_allocator.hpp"

template<typename T>
class Generator {
public:
    using Body = std::function<void(Generator<T>&)>;

    explicit Generator(Body body)
        : body_(std::move(body))
        , finished_(false)
        , stack_(nullptr) {
        
        stack_ = StaticStackPool::allocate();
        
        getcontext(&ctx_coro_);
        ctx_coro_.uc_link = nullptr;
        ctx_coro_.uc_stack.ss_sp = stack_;
        ctx_coro_.uc_stack.ss_size = StaticStackPool::STACK_SIZE;
        ctx_coro_.uc_stack.ss_flags = 0;
        
        makecontext(&ctx_coro_, reinterpret_cast<void(*)()>(coroutine_entry), 1, this);
    }

    ~Generator() {
        if (!finished_ && stack_) {
            StaticStackPool::deallocate(stack_);
        }
    }

    // Запрещаем копирование
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    // Разрешаем перемещение
    Generator(Generator&& other) noexcept
        : ctx_caller_(other.ctx_caller_)
        , ctx_coro_(other.ctx_coro_)
        , body_(std::move(other.body_))
        , value_(std::move(other.value_))
        , finished_(other.finished_)
        , stack_(other.stack_) {
        other.stack_ = nullptr;
        other.finished_ = true;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (!finished_ && stack_) {
                StaticStackPool::deallocate(stack_);
            }
            ctx_caller_ = other.ctx_caller_;
            ctx_coro_ = other.ctx_coro_;
            body_ = std::move(other.body_);
            value_ = std::move(other.value_);
            finished_ = other.finished_;
            stack_ = other.stack_;
            other.stack_ = nullptr;
            other.finished_ = true;
        }
        return *this;
    }

    void yield(T value) {
        value_ = std::move(value);
        swapcontext(&ctx_coro_, &ctx_caller_);
    }

    std::optional<T> next() {
        if (finished_) {
            return std::nullopt;
        }
        swapcontext(&ctx_caller_, &ctx_coro_);
        if (finished_) {
            return std::nullopt;
        }
        T result = std::move(*value_);
        value_.reset();
        return result;
    }

private:
    ucontext_t ctx_caller_;
    ucontext_t ctx_coro_;
    Body body_;
    std::optional<T> value_;
    bool finished_;
    void* stack_;

    static void coroutine_entry(Generator<T>* gen) {
        gen->body_(*gen);
        gen->finished_ = true;
        if (gen->stack_) {
            StaticStackPool::deallocate(gen->stack_);
            gen->stack_ = nullptr;
        }
        // Возвращаем управление, чтобы next() получил nullopt
        swapcontext(&gen->ctx_coro_, &gen->ctx_caller_);
    }
};

#endif // GENERATOR_HPP
