#ifndef PMS_H
#define PMS_H

#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <math.h>
#include <mpi.h>

using namespace std;

typedef enum msg_tag {
    MSG_PIPELINE0,
    MSG_PIPELINE1,
} MSG_TAG;

class Process {
public:
    Process() {
        // MPI must be initialized beforehand
        this->process_cnt = MPI::COMM_WORLD.Get_size();
        this->process_id = MPI::COMM_WORLD.Get_rank();
    }
    ~Process() {
        MPI::Finalize();
    }
protected:
    int process_id;
    int process_cnt;
    int expected_size(int n) {
        return pow(2, n);
    }
private:
};

class Initializer : Process {
public:
    Initializer(string filename = "numbers") : Process() {
        this->filename = filename;
    }

    void run() {
        this->read_input();

        for (size_t i = 0; i < numbers.size(); i++) {
            unsigned char x = numbers[i];
            cout << (int)x << " ";

            MSG_TAG tag = MSG_PIPELINE0;
            if (i % 2 != 0) {
                tag = MSG_PIPELINE1;
            }

            MPI::COMM_WORLD.Send(&x, 1, MPI::UNSIGNED_CHAR, 1, tag);
        }
        cout << endl;
    }
private:
    string filename = "numbers";
    vector<unsigned char> numbers;

    void read_input() {
        ifstream input(this->filename, ios::in | ios::binary);

        while(input.good()) {
            unsigned char n = input.get();
            if(!input.good()) break;

            this->numbers.push_back(n);
        }

        input.close();

        // check input size
        unsigned int expected_total = this->expected_size(this->process_cnt - 1);
        if (this->numbers.size() != expected_total) {
            cerr << "Invalid number of processes for input size. Expected size: " << expected_total << ". Got: " << numbers.size() << endl;
            MPI::COMM_WORLD.Abort(2);
        }
    }
};

class Middleman : Process {
public:
    Middleman() : Process() {
        this->input_size = this->expected_size(this->process_id - 1);
        this->output_size = this->expected_size(this->process_id);
        this->total_size = this->expected_size(this->process_cnt - 1);

        this->reset_available();
    }
    void run() {
        while (processed < this->total_size) {
            if (this->received < this->total_size) {
                this->receive();
            }

            if (this->pipeline0.size() == this->input_size && this->pipeline1.size() == 1) {
                this->can_process = true;
            }

            if (this->can_process) {
                unsigned char x = this->pop_smaller();

                MPI::COMM_WORLD.Send(&x, 1, MPI::UNSIGNED_CHAR, process_id +1, this->next_pipeline);

                this->processed++;
                this->processed_batch++;
                if (this->processed_batch >= this->output_size) {
                    this->processed_batch = 0;
                    this->next_pipeline = this->next_pipeline == MSG_PIPELINE0 ? MSG_PIPELINE1 : MSG_PIPELINE0;
                    this->reset_available();
                }
            }
        }
    }
private:
    unsigned int input_size;
    unsigned int output_size;
    unsigned int total_size;
    // pipelines
    int pipeline0_available;
    int pipeline1_available;
    queue<unsigned char> pipeline0;
    queue<unsigned char> pipeline1;
    // state variables
    unsigned int received = 0;
    bool can_process = false;
    unsigned int processed = 0;
    unsigned int processed_batch = 0;
    MSG_TAG next_pipeline = MSG_PIPELINE0;

    void reset_available() {
        this->pipeline0_available = this->output_size / 2;
        this->pipeline1_available = this->output_size / 2;
    }

    void receive() {
        unsigned char x;
        MPI::Status status;
        MPI::COMM_WORLD.Recv(&x, 1, MPI::UNSIGNED_CHAR, process_id - 1, MPI::ANY_TAG, status);

        if (status.Get_tag() == (int)MSG_PIPELINE0) {
            this->pipeline0.push(x);
        } else {
            this->pipeline1.push(x);
        }

        this->received++;
    }

    unsigned char pop_smaller() {
        unsigned char x;

        if (this->pipeline0_available == 0) {
            x = this->pipeline1.front();
            this->pipeline1.pop();
            this->pipeline1_available--;
        } else if (this->pipeline1_available == 0) {
            x = this->pipeline0.front();
            this->pipeline0.pop();
            this->pipeline0_available--;
        } else if (!this->pipeline0.empty() && this->pipeline1.empty()) {
            x = this->pipeline0.front();
            this->pipeline0.pop();
            this->pipeline0_available--;
        } else if (this->pipeline0.empty() && !this->pipeline1.empty()) {
            x = this->pipeline1.front();
            this->pipeline1.pop();
            this->pipeline1_available--;
        } else if (this->pipeline0.front() < this->pipeline1.front()) {
            x = this->pipeline0.front();
            this->pipeline0.pop();
            this->pipeline0_available--;
        } else {
            x = this->pipeline1.front();
            this->pipeline1.pop();
            this->pipeline1_available--;
        }

        return x;
    }
};

class Finalizer : Process {
public:
    Finalizer() : Process() {
        this->input_size = this->expected_size(this->process_id - 1);
        this->total_size = this->expected_size(this->process_cnt - 1);
    }

    void run() {
        while(this->processed < this->total_size) {
            if(this->received < this->total_size) {
                this->receive();
            }

            if (this->pipeline0.size() == this->input_size && this->pipeline1.size() == 1) {
                this->can_process = true;
            }

            if (this->can_process) {
                unsigned char x = this->pop_smaller();

                this->result.push_back(x);
                this->processed++;
            }
        }
    }

    vector<unsigned char> get_result() {
        return this->result;
    }
private:
    unsigned int input_size;
    unsigned int total_size;
    queue<unsigned char> pipeline0;
    queue<unsigned char> pipeline1;
    vector<unsigned char> result;
    // state variables
    unsigned int received = 0;
    unsigned int processed = 0;
    bool can_process = false;

    void receive() {
        unsigned char x;
        MPI::Status status;
        MPI::COMM_WORLD.Recv(&x, 1, MPI::UNSIGNED_CHAR, process_id - 1, MPI::ANY_TAG, status);

        if (status.Get_tag() == (int)MSG_PIPELINE0) {
            this->pipeline0.push(x);
        } else {
            this->pipeline1.push(x);
        }

        this->received++;
    }

    unsigned char pop_smaller() {
        unsigned char val;

        if (!this->pipeline0.empty() && this->pipeline1.empty()) {
            val = this->pipeline0.front();
            this->pipeline0.pop();
        } else if (this->pipeline0.empty() && !this->pipeline1.empty()) {
            val = this->pipeline1.front();
            this->pipeline1.pop();
        } else if (this->pipeline0.front() < this->pipeline1.front()) {
            val = this->pipeline0.front();
            this->pipeline0.pop();
        } else { val = this->pipeline1.front();
            this->pipeline1.pop();
        }

        return val;
    }
};

#endif
