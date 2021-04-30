#include <iostream>
#include "pms.h"

using namespace std;

int main(int argc, char** argv) {
    // init MPI
    MPI::Init(argc, argv);
    int process_id = MPI::COMM_WORLD.Get_rank();
    int process_cnt = MPI::COMM_WORLD.Get_size();

    if (process_id == 0) {
        auto i = Initializer();
        i.run();
    } else if (process_id == (process_cnt - 1)) {
        auto f = Finalizer();
        f.run();

        auto result = f.get_result();
        for(auto x : result) {
            cout << (int)x << endl;
        }

    } else {
        auto m = Middleman();
        m.run();
    }


    return 0;
}
