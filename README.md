# yolo-code-comments
you can download the code from [github](https://github.com/AlexeyAB/darknet)

我后期的注释是中文的。

​	前一段时间过年那阵子，做了一个小项目，用YOLO检测车辆。于是把yolo的论文v1 v2刷了一遍，然后下了代码，跑了一遍。后期老师说让我好好去读读源代码，我花了几天时间把基于darknet的代码刷了一遍，感受颇多。觉得大神不仅学术棒，代码写的也是非常的好。本来打算早早把这些记录下来，无奈事情多，还上课，一拖再拖。

​	本次重点把自己注释的代码拉出来，希望大家一起学习，肯定有很多错误，希望大神们指正。

YOLO将之前常用的分类问题改为回归问题，将原来的two-stage变成one-stage。



重点注释了 

- **convolutional_layer.c**  卷积层 比较难理解

先上图，可以看一下这个[链接](https://blog.csdn.net/Mrhiuser/article/details/52672824)

 ![im2col](https://github.com/Roc-Ng/yolo-code-comments/blob/master/im2col.png)



- **detection_layer.c** 这个是yolo_v1里面最核心的，计算损失函数
- **dropout_layer.c**
- **gemm.c**  矩阵相乘
- **im2col.c**
- **maxpool_layer.c**
- **region_layer.c**   这个是yolo_v2里面最核心的，计算损失函数。



注释的代码在我的[github](https://github.com/Roc-Ng/yolo-code-comments)





