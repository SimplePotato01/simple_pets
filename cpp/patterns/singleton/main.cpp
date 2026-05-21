#include <iostream>
#include <mutex>

class Logger {
private:
    static Logger* instance;
    static std::mutex mutex;
    Logger() {}
    
public:
    static Logger* getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!instance) {
            instance = new Logger();
        }
        return instance;
    }
    
    void log(const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    }
};

Logger* Logger::instance = nullptr;
std::mutex Logger::mutex;

int main() {
    Logger::getInstance()->log("Hello");
    Logger::getInstance()->log("World");
    return 0;
}
