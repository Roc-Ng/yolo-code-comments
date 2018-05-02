#include "im2col.h"
#include <stdio.h>
float im2col_get_pixel(float *im, int height, int width, int channels,
                        int row, int col, int channel, int pad)
{
    row -= pad;
    col -= pad;

    if (row < 0 || col < 0 ||
        row >= height || col >= width) return 0;
    return im[col + width*(row + height*channel)]; // 通道优先，然后再是一行行走，
	//这样子弄开 最直接 channel*(width*height)+row*(width)+col*1
}

//From Berkeley Vision's Caffe!
//https://github.com/BVLC/caffe/blob/master/LICENSE
void im2col_cpu(float* data_im,
     int channels,  int height,  int width,
     int ksize,  int stride, int pad, float* data_col) 
{
    int c,h,w;
    int height_col = (height + 2*pad - ksize) / stride + 1;//输出的高
    int width_col = (width + 2*pad - ksize) / stride + 1; //输出的宽
	/**************************************************************/
	// 这个地方是要生成一个方块矩阵 其中高为卷积核的大小 即：channels_col
	/*************************************************************/
    int channels_col = channels * ksize * ksize; //卷积核的大小
    for (c = 0; c < channels_col; ++c) {  //这个就是把输入中要卷积的区域拉成这么长的向量，
        int w_offset = c % ksize;  //在卷积核中的第几列
        int h_offset = (c / ksize) % ksize;// 第几行
        int c_im = c / ksize / ksize; //第几个通道
		//两层循环卷积核把图像遍历一遍
        for (h = 0; h < height_col; ++h) {
            for (w = 0; w < width_col; ++w) {
                int im_row = h_offset + h * stride;//卷积核中的位置加上移动的距离
                int im_col = w_offset + w * stride;
                int col_index = (c * height_col + h) * width_col + w;//位置索引
                data_col[col_index] = im2col_get_pixel(data_im, height, width, channels,
                        im_row, im_col, c_im, pad);//复制一个点过来
            }
        }
    }
}

