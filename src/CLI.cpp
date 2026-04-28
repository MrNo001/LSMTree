#include "../h/CLI.hpp"
#include <iostream>

CLI::CLI() : store_(std::filesystem::path("data")) {}

CLI::~CLI() {}

void CLI::run(){
    while (true) {
        printOptions();
        int option;
        std::cin >> option;
        handleOption(option);
    }
}

void CLI::printOptions(){
    std::cout << "1. Insert" << std::endl;
    std::cout << "2. Remove" << std::endl;
    std::cout << "3. Search" << std::endl;
    std::cout << "4. Print" << std::endl;
    std::cout << "5. Exit" << std::endl;
}

void CLI::handleOption(int option){
    switch (option) {
        case 1:
            handleInsert();
            break;
        case 2:
            handleRemove();
            break;
        case 3:
            handleSearch();
            break;
        case 4:
            handlePrint();
            break;
        case 5:
            handleExit();
            break;
        default:
            std::cout << "Invalid option" << std::endl;
            break;
    }
}

void CLI::handleInsert(){
    std::cout << "Enter key: ";
    int key;
    std::cin >> key;
    std::cout << "Enter value: ";
    int value;
    std::cin >> value;
    this->store_.insert(key, value);
}

void CLI::handleRemove(){
    std::cout << "Enter key: ";
    int key;
    std::cin >> key;
    this->store_.remove(key);
}

void CLI::handleSearch(){
    std::cout << "Enter key: ";
    int key;
    std::cin >> key;
    int value = this->store_.search(key);
    std::cout << "Value: " << value << std::endl;
}

void CLI::handlePrint(){
    this->store_.print();
}

void CLI::handleExit(){
    std::cout << "Exiting..." << std::endl;
    exit(0);
}
