#ifndef MATRIX_H
#define MATRIX_H

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <LoggingUtils.h>

extern Logger l;

template <typename T>
class Matrix {
    private:
        T* buff;
        int num_rows, num_cols;
        std::string name;
    public:
        Matrix(int n, int m, std::string name) {
            //std::cout << "Matrix \"" << name <"< "\" was created\n";

            if (n <= 0 || m <= 0 ) {
                //throw error
                throw std::runtime_error("Dimentions");
            }

            buff = new T[n * m];

            num_rows = n;
            num_cols = m;

            this->name = name;
        }

        Matrix(const Matrix<T>& m) {
            this->num_rows = m.num_rows;
            this->num_cols = m.num_cols;
            
            this->buff = new T[num_rows * num_cols];

            for (int i = 0; i < num_rows * num_cols; i++) {
                buff[i] = m.buff[i];
            }

            this->name = m.name;

            // for (int i = 0; i < num_rows; i++) {
            //     for (int j = 0; j < num_cols) {
            //         buff[sizeof(T) * i + j] = m.get(i, j);
            //     }
            // }

            
        }

        ~Matrix() {
            //std::cout << "Matrix \"" << name << "\" was destroyed\n";
            delete[] buff;
        }

        Matrix<T>& operator=(const Matrix<T>& m) {
            this->num_rows = m.num_rows;
            this->num_cols = m.num_cols;

            if (this == &m) {
                return *this;
            }

            delete[] buff;
            this->buff = new T[num_rows * num_cols];

            for (int i = 0; i < num_rows * num_cols; i++) {
                buff[i] = m.buff[i];
            }

            this->name = m.name;

            return *this;
        }

        T get(int i, int j) {
            if (! (0 <= i && i < num_rows)) {
                throw std::invalid_argument("accessing index out of bounds of matrix");
            }

            if (! (0 <= j && j < num_cols)) {
                throw std::invalid_argument("accessing index out of bounds of matrix");
            }

            return buff[num_cols*i + j];
        }

        void set(int i, int j, T val) {
            if (! (0 <= i && i < num_rows)) {
                throw std::invalid_argument("accessing index out of bounds of matrix");
            }

            if (! (0 <= j && j < num_cols)) {
                throw std::invalid_argument("accessing index out of bounds of matrix");
            }

            buff[num_cols*i + j] = val;
        }

        int numOfRows() {
            return num_rows;
        }
        int numOfCols() {
            return num_cols;
        }

        void printBuff() {

            std::cout << "\n-------------------------------\n";
            for (int i = 0; i < num_cols * num_rows; i++) {
                std::cout << buff[i] << " ";
            }
            std::cout << "\n-------------------------------\n";
        }
};

template <typename T>
void printMatrix(Matrix<T>& mat) {
    int n = mat.numOfRows();
    int m = mat.numOfCols();


    for (int i = 0; i < n; i++) {
        std::string line;
        for (int j = 0; j < m; j++) {
            line += std::to_string(mat.get(i, j)) + " "; 
        }

        l.log(line);
    }
} 

Matrix<bool> loadTMatrix(std::string fileName, std::string matrixName);
Matrix<bool> booleanProduct(Matrix<bool>& m1, Matrix<bool>& m2);
Matrix<bool> matrixUnion(Matrix<bool>& mat1, Matrix<bool>& mat2);

#endif
