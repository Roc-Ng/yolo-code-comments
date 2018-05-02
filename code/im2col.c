#include "im2col.h"
#include <stdio.h>
float im2col_get_pixel(float *im, int height, int width, int channels,
                        int row, int col, int channel, int pad)
{
    row -= pad;
    col -= pad;

    if (row < 0 || col < 0 ||
        row >= height || col >= width) return 0;
    return im[col + width*(row + height*channel)]; // ͨ�����ȣ�Ȼ������һ�����ߣ�
	//������Ū�� ��ֱ�� channel*(width*height)+row*(width)+col*1
}

//From Berkeley Vision's Caffe!
//https://github.com/BVLC/caffe/blob/master/LICENSE
void im2col_cpu(float* data_im,
     int channels,  int height,  int width,
     int ksize,  int stride, int pad, float* data_col) 
{
    int c,h,w;
    int height_col = (height + 2*pad - ksize) / stride + 1;//����ĸ�
    int width_col = (width + 2*pad - ksize) / stride + 1; //����Ŀ�
	/**************************************************************/
	// ����ط���Ҫ����һ��������� ���и�Ϊ����˵Ĵ�С ����channels_col
	/*************************************************************/
    int channels_col = channels * ksize * ksize; //����˵Ĵ�С
    for (c = 0; c < channels_col; ++c) {  //������ǰ�������Ҫ���������������ô����������
        int w_offset = c % ksize;  //�ھ�����еĵڼ���
        int h_offset = (c / ksize) % ksize;// �ڼ���
        int c_im = c / ksize / ksize; //�ڼ���ͨ��
		//����ѭ������˰�ͼ�����һ��
        for (h = 0; h < height_col; ++h) {
            for (w = 0; w < width_col; ++w) {
                int im_row = h_offset + h * stride;//������е�λ�ü����ƶ��ľ���
                int im_col = w_offset + w * stride;
                int col_index = (c * height_col + h) * width_col + w;//λ������
                data_col[col_index] = im2col_get_pixel(data_im, height, width, channels,
                        im_row, im_col, c_im, pad);//����һ�������
            }
        }
    }
}

