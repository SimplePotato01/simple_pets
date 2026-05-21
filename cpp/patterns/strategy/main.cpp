#include <iostream>
#include <memory>

class SortStrategy {
public:
    virtual void sort(int* arr, int size) = 0;
    virtual ~SortStrategy() = default;
};

class BubbleSort : public SortStrategy {
public:
    void sort(int* arr, int size) override {
        std::cout << "Сортировка пузырьком: ";
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size - i - 1; j++) {
                if (arr[j] > arr[j + 1]) {
                    std::swap(arr[j], arr[j + 1]);
                }
            }
        }
    }
};

class QuickSort : public SortStrategy {
public:
    void sort(int* arr, int size) override {
        std::cout << "Быстрая сортировка: ";
        quickSort(arr, 0, size - 1);
    }
    
private:
    void quickSort(int* arr, int left, int right) {
        if (left >= right) return;
        int pivot = arr[(left + right) / 2];
        int i = left, j = right;
        while (i <= j) {
            while (arr[i] < pivot) i++;
            while (arr[j] > pivot) j--;
            if (i <= j) std::swap(arr[i++], arr[j--]);
        }
        quickSort(arr, left, j);
        quickSort(arr, i, right);
    }
};

class Sorter {
private:
    std::unique_ptr<SortStrategy> strategy;
    
public:
    void setStrategy(std::unique_ptr<SortStrategy> s) {
        strategy = std::move(s);
    }
    
    void sortArray(int* arr, int size) {
        strategy->sort(arr, size);
        for (int i = 0; i < size; i++) {
            std::cout << arr[i] << " ";
        }
        std::cout << std::endl;
    }
};

int main() {
    Sorter sorter;
    int arr1[] = {5, 2, 8, 1, 9};
    int arr2[] = {7, 3, 6, 4, 2};
    
    sorter.setStrategy(std::make_unique<BubbleSort>());
    sorter.sortArray(arr1, 5);
    
    sorter.setStrategy(std::make_unique<QuickSort>());
    sorter.sortArray(arr2, 5);
    
    return 0;
}
