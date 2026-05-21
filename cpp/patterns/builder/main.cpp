#include <iostream>
#include <string>
#include <vector>

class Pizza {
public:
    std::string dough;
    std::string sauce;
    std::vector<std::string> toppings;
    
    void show() {
        std::cout << "Pizza: " << dough << ", " << sauce << ", ";
        for (const auto& t : toppings) std::cout << t << " ";
        std::cout << std::endl;
    }
};

class PizzaBuilder {
private:
    Pizza pizza;
    
public:
    PizzaBuilder& setDough(const std::string& dough) {
        pizza.dough = dough;
        return *this;
    }
    
    PizzaBuilder& setSauce(const std::string& sauce) {
        pizza.sauce = sauce;
        return *this;
    }
    
    PizzaBuilder& addTopping(const std::string& topping) {
        pizza.toppings.push_back(topping);
        return *this;
    }
    
    Pizza build() {
        return pizza;
    }
};

int main() {
    Pizza pizza = PizzaBuilder()
        .setDough("thin")
        .setSauce("tomato")
        .addTopping("cheese")
        .addTopping("mushrooms")
        .build();
    
    pizza.show();
    return 0;
}
