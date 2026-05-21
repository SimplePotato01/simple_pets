#include <iostream>
#include <vector>
#include <memory>

class Observer {
public:
    virtual void update(int value) = 0;
    virtual ~Observer() = default;
};

class Subject {
private:
    std::vector<Observer*> observers;
    int state;
    
public:
    void attach(Observer* obs) {
        observers.push_back(obs);
    }
    
    void setState(int value) {
        state = value;
        notify();
    }
    
    void notify() {
        for (auto obs : observers) {
            obs->update(state);
        }
    }
};

class Display : public Observer {
private:
    std::string name;
    
public:
    Display(const std::string& n) : name(n) {}
    
    void update(int value) override {
        std::cout << name << " получил: " << value << std::endl;
    }
};

int main() {
    Subject subject;
    Display d1("Экран 1");
    Display d2("Экран 2");
    
    subject.attach(&d1);
    subject.attach(&d2);
    
    subject.setState(42);
    subject.setState(100);
    
    return 0;
}
