#include <iostream>
#include <memory>

class Coffee {
public:
    virtual double cost() = 0;
    virtual std::string description() = 0;
    virtual ~Coffee() = default;
};

class SimpleCoffee : public Coffee {
public:
    double cost() override { return 5.0; }
    std::string description() override { return "Кофе"; }
};

class MilkDecorator : public Coffee {
private:
    std::unique_ptr<Coffee> coffee;
    
public:
    MilkDecorator(std::unique_ptr<Coffee> c) : coffee(std::move(c)) {}
    
    double cost() override {
        return coffee->cost() + 1.5;
    }
    
    std::string description() override {
        return coffee->description() + " + молоко";
    }
};

class SugarDecorator : public Coffee {
private:
    std::unique_ptr<Coffee> coffee;
    
public:
    SugarDecorator(std::unique_ptr<Coffee> c) : coffee(std::move(c)) {}
    
    double cost() override {
        return coffee->cost() + 0.5;
    }
    
    std::string description() override {
        return coffee->description() + " + сахар";
    }
};

int main() {
    auto coffee = std::make_unique<SimpleCoffee>();
    std::cout << coffee->description() << " = " << coffee->cost() << std::endl;
    
    auto milkCoffee = std::make_unique<MilkDecorator>(std::move(coffee));
    std::cout << milkCoffee->description() << " = " << milkCoffee->cost() << std::endl;
    
    auto sugarMilkCoffee = std::make_unique<SugarDecorator>(std::move(milkCoffee));
    std::cout << sugarMilkCoffee->description() << " = " << sugarMilkCoffee->cost() << std::endl;
    
    return 0;
}
