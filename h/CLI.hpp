#ifndef CLI_HPP
#define CLI_HPP

#include "LSMStore.hpp"

class CLI {
    public:
    CLI();
    ~CLI();
    void run();
    private:
    LSMStore store_;
    void printOptions();
    void handleOption(int option);

    void handleInsert();
    void handleRemove();
    void handleSearch();
    void handlePrint();
    void handleExit();

};



#endif 