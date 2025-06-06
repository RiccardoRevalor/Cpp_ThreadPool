#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <cmath>
#include "threadpool.h"

using namespace std;

mutex m;

double determinant(const std::vector<std::vector<double>>& matrix) {
    if (matrix.empty()) {
        return 1; //det of a 0x0 matrix is 1
    }

    const size_t n = matrix.size();
    if (n != matrix[0].size()) {
        throw std::invalid_argument("La matrice deve essere quadrata per calcolare il determinante.");
    }


    
    //1x1 matrix
    if (n == 1) {
        return matrix[0][0];
    }

    //2x2 matrix
    if (n == 2) {
        return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    }

    //recursion approach with larger matrices
    double det = 0.0;

    for (size_t j = 0; j < n; ++j) {
        std::vector<std::vector<double>> submatrix(n - 1, std::vector<double>(n - 1));
        for (size_t r = 1; r < n; ++r) {
            size_t sub_col = 0;
            for (size_t c = 0; c < n; ++c) {
                if (c == j) {
                    continue; 
                }
                submatrix[r - 1][sub_col] = matrix[r][c];
                sub_col++;
            }
        }

        double sign = std::pow(-1.0, j);

        det += sign * matrix[0][j] * determinant(submatrix);
    }

    return det;
}


void computeMatrixDet(const string &inputfile, const string &outputfile){
    ifstream input(inputfile);
    if (!input){
        cerr << "Error opening input file." << endl;
        exit(-1);
    }

    vector<vector<double>> matrix;
    string line;
    //read matrix from file
    while(getline(input, line)){
        double element;

        stringstream ss(line);

        vector<double> row;

        while (ss >> element){
            row.emplace_back(element);
        }

        matrix.emplace_back(row);
      
    }

    input.close();

    //compute determinant
    double det = determinant(ref(matrix));

    //save in output
    {
        lock_guard<mutex> lock(m);
        ofstream output(outputfile, ios::app); //append, not truncate!

        if (!output){
            cerr << "Error opening output file." << endl;
            exit(-1);
        }

        output << inputfile << ": " << det << endl;

        output.close();

    }

    
}


int main(int argc, char **argv){


    if (argc < 3){
        cerr << "Usage: prog.exe N M" << endl;
        exit(-1);
    }

    int N = atoi(argv[1]);
    int M = atoi(argv[2]);

    string outputfile("fileOut.txt");

    {
        ThreadPool threadpool(N);

        for (int i = 0; i < M; i++){
            string inputfile("fileIn-" + to_string(i+1) + ".txt");

            //threadpool.enqueue wants a F &&f with no args, so encapsulate the function into a lambda which is a void()
            threadpool.enqueue([inputfile, outputfile] {
                computeMatrixDet(inputfile, outputfile);
            });
        }
    } //exit from this scope will call te destructor of the ThreadPool, which will join all consumer threads and wait for them


    return 0;
}