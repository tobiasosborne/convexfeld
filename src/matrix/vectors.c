/**
 * @file vectors.c
 * @brief Vector operations for simplex computations.
 *
 * M4.1.4: Implements cxf_dot_product and cxf_vector_norm functions.
 */

#include <stddef.h>
#include <math.h>

/**
 * @brief Compute dense dot product of two vectors.
 *
 * Computes x[0]*y[0] + x[1]*y[1] + ... + x[n-1]*y[n-1].
 *
 * @param x First vector (n elements).
 * @param y Second vector (n elements).
 * @param n Vector length.
 * @return Dot product value, or 0.0 if n <= 0.
 */
double cxf_dot_product(const double *x, const double *y, int n) {
    double sum = 0.0;
    int i;

    if (n <= 0 || x == NULL || y == NULL) {
        return 0.0;
    }

    for (i = 0; i < n; i++) {
        sum += x[i] * y[i];
    }

    return sum;
}

/**
 * @brief Compute sparse-dense dot product.
 *
 * Computes sum of x_values[k] * y_dense[x_indices[k]] for k in [0, x_nnz).
 * Efficient when sparse vector has few non-zeros.
 *
 * @param x_indices Indices of non-zeros in sparse vector.
 * @param x_values Values of non-zeros in sparse vector.
 * @param x_nnz Number of non-zeros.
 * @param y_dense Dense vector to dot against.
 * @return Dot product value, or 0.0 if x_nnz <= 0.
 */
double cxf_dot_product_sparse(const int *x_indices, const double *x_values,
                              int x_nnz, const double *y_dense) {
    double sum = 0.0;
    int k;

    if (x_nnz <= 0) {
        return 0.0;
    }

    if (x_indices == NULL || x_values == NULL || y_dense == NULL) {
        return 0.0;
    }

    for (k = 0; k < x_nnz; k++) {
        int idx = x_indices[k];
        sum += x_values[k] * y_dense[idx];
    }

    return sum;
}

/**
 * @brief Compute vector norm.
 *
 * @param x Input vector (n elements).
 * @param n Vector length.
 * @param norm_type Norm type: 0=L_inf, 1=L1, 2=L2.
 * @return Norm value (non-negative), or 0.0 if n <= 0.
 */
double cxf_vector_norm(const double *x, int n, int norm_type) {
    int i;

    if (n <= 0 || x == NULL) {
        return 0.0;
    }

    if (norm_type == 1) {
        /* L1 norm: sum of absolute values */
        double sum = 0.0;
        for (i = 0; i < n; i++) {
            sum += fabs(x[i]);
        }
        return sum;
    } else if (norm_type == 2) {
        /* L2 norm: sqrt of sum of squares */
        double sum = 0.0;
        for (i = 0; i < n; i++) {
            sum += x[i] * x[i];
        }
        return sqrt(sum);
    } else {
        /* L_inf norm (default): maximum absolute value */
        double max_val = 0.0;
        for (i = 0; i < n; i++) {
            double abs_xi = fabs(x[i]);
            if (abs_xi > max_val) {
                max_val = abs_xi;
            }
        }
        return max_val;
    }
}
