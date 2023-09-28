#include "Master.h"
int main() {
    core::RunServer(getenv("CORE_MASTER_ADDR"));
    return 0;
}