#include <MatrixUtils.h>
#include <Config.h>
#include <LoggingUtils.h>

int boolCount = 1;
int unionCount = 1;

Matrix<bool> loadTMatrix(std::string fileName, std::string matrixName) {
     std::ifstream strm(config::CONFIG_DIR + "/" + fileName);
    
    if (strm.is_open()) {
        l.log("Using ", fileName, " as topology data");
    } else {
        throw std::runtime_error("Failed to open file " + config::CONFIG_DIR + "/" + fileName);
    }
    
    int n;
    strm >> n;
    //std::cout << n << "\n";

    Matrix<bool> topology(n, n, matrixName);

    for (int i = 0; i < n; i++) {
        //std::cout << "i: " << i << "\n";

        for (int j = 0; j < n; j++) {
            int temp;
            strm >> temp;
            //std::cout << "i: " << i << " j: " << j << " temp: " << (bool)temp << "\n";
            topology.set(i, j, (bool) temp);
        }
    }

    //printMatrix(topology);
    //topology.printBuff();

    return topology;
}

Matrix<bool> booleanProduct(Matrix<bool>& m1, Matrix<bool>& m2) {
    if (m1.numOfCols() != m2.numOfRows()) {
        throw std::invalid_argument("Attempting to boolean product matrixes of incompatible dimenetions");
    }

    int l = m1.numOfCols();

    int n = m1.numOfRows();
    int m = m2.numOfCols();

    Matrix<bool> result(m1.numOfRows(), m2.numOfCols(), "Boolean Matrix " + std::to_string(boolCount));
    boolCount++;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            bool acc = false;
            
            for (int k = 0; k < l; k++) {
                bool temp = m1.get(i, k) && m2.get(k, j);
                acc = acc || temp;
            }

            result.set(i, j, acc);
        }
    }

    return result;
}

Matrix<bool> matrixUnion(Matrix<bool>& mat1, Matrix<bool>& mat2) {
    int n1 = mat1.numOfRows();
    int n2 = mat2.numOfRows();
    int m1 = mat1.numOfCols();
    int m2 = mat2.numOfCols();

    if (! (n1 == n2 && m1 == m2)) {
        throw std::invalid_argument("Attempting to union product matrixes of incompatible dimenetions");
    }

    Matrix<bool> result(n1, m1, "Union Matrix " + std::to_string(unionCount));
    unionCount++;

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < m1; j++) {
            result.set(i, j, mat1.get(i, j) || mat2.get(i, j));
        }
    }
    
    return result;
}