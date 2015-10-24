// Skin spots detection and skin ranking
// @author: lixihua@126.com
// @date:   2015/09/20
// pixel based image segmentatin, wavelet features
//

#include "stdafx.h"
#include "opencv2/opencv.hpp"
#include "Wavelet.h"
#include "FilesRead.h"
#include "LearnAlgorithms.h"

using namespace std;
using namespace cv;
const string DATASPATH = "../datas";
const string INFOPATH  = "../datas/标注相关信息.txt";
const int H = 2592;
const int W = 3872;
const int NLAYER = 1;

int _tmain(int argc, _TCHAR* argv[])
{
	// Info Map
	map<string, InfoStruct> InfoMap;
	GetLabelInfo(INFOPATH, InfoMap);

	// All Files
	vector<string> files;
	GetFiles(DATASPATH, "", files);

	for (int i = 0; i < files.size(); i++)
	{
		string filename = files[i].c_str();
		if (-1 != filename.find(".txt"))  continue;
		if (-1 != filename.find(".xlsx")) continue;
		vector<string> splited;
		splited = str_split(filename, "/");
		map<string, InfoStruct>::iterator it = InfoMap.find(splited[3]);
		if (it != InfoMap.end()) {
			if (-1 != filename.find(".JPG")) {
				it->second.OriImage = filename;
			}
			if (-1 != filename.find("主要区域")) {
				it->second.SpotImage = filename;
			}
			if (-1 != filename.find("正常肤色")) {
				it->second.NormImage = filename;
			}
			if (-1 != filename.find("最深区域") || -1 != filename.find("最深部位")) {
				it->second.DarkImage = filename;
			}
		}
	}

	// Training Data
	Mat data_mat  = Mat::zeros(H*W*40/16, 12, CV_32FC1);
	Mat res_mat   = Mat::zeros(H*W*40/16, 1, CV_32FC1);

	Wavelet wavelet(NLAYER); // 初始化小波类
	Mat ori, resized, des, spot, gray;
	int i = 0; // for label
	int j = 0; // for train data
	namedWindow("TEST", CV_WINDOW_NORMAL);
	Mat lena = imread("lena.jpg");
	Mat show = wavelet.WaveletImage(lena);
	//imshow("TEST", show);
	//waitKey();
	int frame_num = 0;
	int train_pos = 0;
	int train_neg = 0;
	for (map<string, InfoStruct>::iterator it = InfoMap.begin(); it != InfoMap.end(); ++it) {
		// wavelet transform
		ori = imread(it->second.OriImage, CV_LOAD_IMAGE_COLOR);
		frame_num++;
		if (frame_num >= 41) break;
		Mat ori_roi = ori(Rect(int(0.25*W), int(0.25*H), int(0.5*W), int(0.5*H)));
		resize(ori_roi, resized, Size(W/4, H/4), 0, 0, CV_INTER_LINEAR);
		des = wavelet.WaveletImage(ori_roi);
		//imshow("TEST", des);
		//waitKey();
		for (int h = 0; h < des.rows/2; h++) {
			for (int w = 0; w < des.cols/2; w++) {
				data_mat.at<float>(j, 0) = float(des.at<Vec3b>(h, w)[0])/255;
				data_mat.at<float>(j, 1) = float(des.at<Vec3b>(h, w)[1])/255;
				data_mat.at<float>(j, 2) = float(des.at<Vec3b>(h, w)[2])/255;
				data_mat.at<float>(j, 3) = float(des.at<Vec3b>(h + H/4, w)[0])/255;
				data_mat.at<float>(j, 4) = float(des.at<Vec3b>(h + H/4, w)[1])/255;
				data_mat.at<float>(j, 5) = float(des.at<Vec3b>(h + H/4, w)[2])/255;
				data_mat.at<float>(j, 6) = float(des.at<Vec3b>(h, w + W/4)[0])/255;
				data_mat.at<float>(j, 7) = float(des.at<Vec3b>(h, w + W/4)[1])/255;
				data_mat.at<float>(j, 8) = float(des.at<Vec3b>(h, w + W/4)[2])/255;
				data_mat.at<float>(j, 9) = float(des.at<Vec3b>(h + H/4, w + W/4)[0])/255;
				data_mat.at<float>(j, 10) = float(des.at<Vec3b>(h + H/4, w + W/4)[1])/255;
				data_mat.at<float>(j, 11) = float(des.at<Vec3b>(h + H/4, w + W/4)[2])/255;
				j++;
			}
		}

		// segmentation info
		ori = imread(it->second.SpotImage);
		ori_roi = ori(Rect(int(0.25*W), int(0.25*H), int(0.5*W), int(0.5*H)));
		resize(ori_roi, spot, Size(W/4, H/4), 0, 0, CV_INTER_LINEAR);
 		cvtColor(spot, gray, CV_BGR2GRAY);
		//imshow("TEST", gray);
		imshow("TEST", spot+resized);
		waitKey(5);
		for (int h = 0; h < gray.rows; h++) {
			for (int w = 0; w < gray.cols; w++) {
				if (int(gray.at<uchar>(h, w)) >= 100) 
				{
					res_mat.at<float>(i, 0) = 1; // 正样本
					train_pos++;
				}
				if (int(gray.at<uchar>(h, w)) < 100)
				{
					res_mat.at<float>(i, 0) = 0; // 负样本
					train_neg++;
				}
				i++;
			}
		}
		cout << "frame " << frame_num << " ok!" << endl;
	}
	cout << "Train pos: " << train_pos << " Train neg: " << train_neg << endl;

	// 训练数据特征的归一化
	// 方法一：x1,x2,..,xn -> xi=xi/(x1^2+x2^2+...+xn^2)
	//Mat tmp1, tmp2, tmp3;
	//pow(data_mat, 2, tmp1);
	//reduce(tmp1, tmp2, 0, CV_REDUCE_SUM);
	//sqrt(tmp2, tmp2);
	//repeat(tmp2, data_mat.rows, 1, tmp3);
	//divide(data_mat, tmp3, data_mat);
	// 方法二：同一特征值采用减均值的操作
	Mat tmp1, tmp2;
	reduce(data_mat, tmp1, 0, CV_REDUCE_AVG);
	cout << "average vector: " << tmp1 << endl;;
	repeat(tmp1, data_mat.rows, 1, tmp2);
	data_mat = data_mat - tmp2;
	reduce(res_mat, tmp1, 0, CV_REDUCE_AVG);
	cout << "average label: " << tmp1 << endl;

	// Model Training	
	CvSVMParams params;
	params.svm_type    = CvSVM::C_SVC;
	params.kernel_type = CvSVM::RBF;
	params.term_crit   = cvTermCriteria(CV_TERMCRIT_ITER, 100, 2e-4);
	params.C = 12.5;
	params.gamma = 0.5;
	Mat1f weights(1, 2);
	weights(0, 0) = 0.2;
	weights(0, 1) = 0.8;
	CvMat weight = weights;
	params.class_weights = &weight;

	string save_name = "svm_model.xml";
	TrainModel(data_mat, res_mat, params, save_name);

	CvSVM svm;
	svm.load(save_name.c_str());
	CvSVMParams params_re = svm.get_params();
	cout << "weights: " << params_re.class_weights << endl;
	cout << "surort vectors:" << endl;
	for (int i = 0; i < svm.get_support_vector_count(); i++)
	{
		cout << svm.get_support_vector(i) << endl;
	}
	int false_alarm = 0;
	int miss = 0;
	
	for (int i = 0; i < data_mat.rows; i++) {
		int ret = int(svm.predict(data_mat.row(i)));
		if (int(res_mat.at<float>(i, 0)) >= 0.5 && ret < 0.5) {
			miss++;
		}
		if (int(res_mat.at<float>(i, 0)) < 0.5 && ret >= 0.5) {
			false_alarm++;
		}
	}

	cout << "miss rate: " << float(miss)/train_pos << endl;
	cout << "false alarm rate: " << float(false_alarm)/train_neg << endl;

	// draw on original image
	frame_num = 0;
	for (map<string, InfoStruct>::iterator it = InfoMap.begin(); it != InfoMap.end(); ++it) {
		Mat ori = imread(it->second.OriImage, CV_LOAD_IMAGE_COLOR);
		frame_num++;
		if (frame_num >= 41) break;
		Mat ori_roi = ori(Rect(int(0.25*W), int(0.25*H), int(0.5*W), int(0.5*H)));
		Mat resized_roi;
		resize(ori_roi, resized_roi, Size(W/4, H/4), 0, 0, CV_INTER_LINEAR);
		int j = 0;
		for (int i = (frame_num - 1)*H*W/16; i < frame_num*H*W/16; i++)
		{
			Point mark_point = Point(j%int(W/4), j/int(W/4));
			int ret = int(svm.predict(data_mat.row(i)));
			if (int(res_mat.at<float>(i, 0)) > 0.5) {
				//circle(resized_roi, mark_point, 1, CV_RGB(0, 255, 0), 1, 8, 0);
			}
			if (ret > 0.5) {
				circle(resized_roi, mark_point, 1, CV_RGB(255, 0, 0), 1, 8, 0);
			}
			j++;
		}
		imshow("TEST", resized_roi);
		waitKey(5);
	}

 	return 0;
}

