#include <iostream>
#include <memory>
#include <string>

class Animal {
public:
	virtual void speak() const = 0;
	virtual ~Animal() = default;
};

class Dog : public Animal {
public:
    void speak() const override {
        std::cout << "Bark" << std::endl;
    }
};

class Cat : public Animal {
public:
    void speak() const override {
        std::cout << "Cat`s sound" << std::endl;
    }
};

class Cow : public Animal {
public:
    void speak() const override {
        std::cout << "Muuu" << std::endl;
    }
};

class AnimalFactory {
public:
    static std::unique_ptr<Animal> createAnimal(const std::string& type) {
        if (type == "dog") {
            return std::make_unique<Dog>();
        } else if (type == "cat") {
            return std::make_unique<Cat>();
        } else if (type == "cow") {
            return std::make_unique<Cow>();
        }
        return nullptr;
    }
};

int main() {
    auto animal1 = AnimalFactory::createAnimal("dog");
    auto animal2 = AnimalFactory::createAnimal("cat");
    auto animal3 = AnimalFactory::createAnimal("cow");
    
    animal1->speak();
    animal2->speak();
    animal3->speak();
    
    return 0;
}
