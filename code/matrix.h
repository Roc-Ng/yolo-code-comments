#ifndef MATRIX_H
#define MATRIX_H
////这里rows是一次加载到内存中的样本的个数（batch*net.subdivisions），cols就是样本的维度，**vals指向的是样本的值
typedef struct matrix{
    int rows, cols;
    float **vals;
} matrix;

matrix make_matrix(int rows, int cols);
void free_matrix(matrix m);
void print_matrix(matrix m);

matrix csv_to_matrix(char *filename);
void matrix_to_csv(matrix m);
matrix hold_out_matrix(matrix *m, int n);
float matrix_topk_accuracy(matrix truth, matrix guess, int k);
void matrix_add_matrix(matrix from, matrix to);
void scale_matrix(matrix m, float scale);
matrix resize_matrix(matrix m, int size);

float *pop_column(matrix *m, int c);

#endif
