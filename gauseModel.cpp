#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include "gauseModel.h"
#include "overlap.h"

/*
2015/12/24���Աʼǣ�
q1. �����ж�D�ļ��������⣺�����ڷ�ĸ�ϣ���ᵼ�·���ԽС��DԽ�󣻶�ʵ������Ƿ���ԽС��˵���ĵ�������Ǳ���
a1: ������
q2. �ñ���ģ�ͺ͵�ǰ֡ȥ���㵥Ӧ����ʱ�����ȶ�������ǰһ֡�͵�ǰ֡ȥ���㵥Ӧ����ʱ��ѧϰ���ı�����ʵ�ʵı������Ǵ���һ���Ĵ�λ
q3. �����ٶ�̫��
q4. mask������ʾ������
*/


//��������
extern Mat lap;

using namespace cv;
using namespace std;

//����������˳��:��ֵ�� ��� ��������
//input: gray
bool GaussModel::initial(Mat src)
{
	gm = Mat::zeros(src.size(), CV_32FC3);
	src.copyTo(gray);
	if (src.empty())
		return false;
	else
	{
		for (size_t i = 0; i < gm.rows; i++)
		{
			Vec3f* gmData = gm.ptr<Vec3f>(i);
			uchar* srcData = src.ptr<uchar>(i);
			for (size_t j = 0; j < gm.cols; j++)
			{
				gmData[j].val[0] = srcData[j];
				gmData[j].val[1] = variance;
				gmData[j].val[2] = 1.f;
			}
		}
		mask = Mat::ones(src.size(), CV_16UC1);
		return true;
	}
}

void GaussModel::updateModel(Mat cur, Mat H, Mat& dst)
{
	//Mat warpFrame;
	//warpAffine(gray, warpFrame, H, gray.size(), INTER_LINEAR);
	//Mat mask(cur.size(), CV_8UC1);	//����
	updateMask(H);
	dst = Mat::zeros(cur.size(), CV_8UC1);
	//Mat iH;
	//iH = H.inv(); //����
	for (size_t i = 0; i < cur.rows; i++)
	{
		ushort* maskData = mask.ptr<ushort>(i);
		uchar* curData = cur.ptr<uchar>(i);
		uchar* dstData = dst.ptr<uchar>(i);
		for (size_t j = 0; j < cur.cols; j++)
		{
			if (maskData[j] > 1)	//�ص�����
			{
				Point2f bk, bkReal;
				//bk.x = j * H.at<double>(1, 1) - i * H.at<double>(0, 1) + H.at<double>(1, 2) * H.at<double>(0, 1) - H.at<double>(0, 2) * H.at<double>(1, 1);
				//bk.y = i * H.at<double>(0, 0) - j * H.at<double>(1, 0) + H.at<double>(0, 2) * H.at<double>(1, 0) - H.at<double>(1, 2) * H.at<double>(0, 0);
				Mat srcMat = Mat::zeros(3, 1, CV_64FC1);
				srcMat.at<double>(0, 0) = j;
				srcMat.at<double>(1, 0) = i;
				srcMat.at<double>(2, 0) = 1.0;
				Mat warpMat = H * srcMat;
				bk.x = warpMat.at<double>(0, 0) / warpMat.at<double>(2, 0);
				bk.y = warpMat.at<double>(1, 0) / warpMat.at<double>(2, 0);
				//��������
				float difMin = INFINITY;
				float neighbor[3] = {-1, 0, 1};
				for (uchar m = 0; m < 3; m++)
				{
					float neighborX = bk.x + neighbor[m];
					if (neighborX >= 0 && neighborX < cur.cols - 1)
					{
						for (uchar n = 0; n < 3; n++)
						{
							float neighborY = bk.y + neighbor[n];
							if (neighborY >= 0 && neighborY < cur.rows - 1)
							{
								float dif = ((float)curData[j] - gm.at<Vec3f>(neighborY, neighborX).val[0]) * ((float)curData[j] - gm.at<Vec3f>(neighborY, neighborX).val[0]) * gm.at<Vec3f>(neighborY, neighborX).val[1];
								if (gm.at<Vec3f>(neighborY, neighborX).val[1] < 1e-6 && (float)curData[j] - gm.at<Vec3f>(neighborY, neighborX).val[0] < 1e-6) //�������0
									dif = 0;
								float temp = gm.at<Vec3f>(neighborY, neighborX).val[1];
								if (dif < difMin)
								{
									difMin = dif;
									bkReal = Point2f(neighborX, neighborY);
								}
							}
							else
								continue;
						}
					}
					else
						continue;
				}
				//�ж���ǰ���㻹�Ǳ�����
				if (difMin < 2.5)//���ڱ����㣬 ���²���
				{
					float alpha = gm.at<Vec3f>(bkReal.y, bkReal.x).val[2];
					if (bkReal.x >= 0 && bkReal.x <= gm.cols - 1 && bkReal.y >= 0 && bkReal.y < gm.rows - 1)
					{
						gm.at<Vec3f>(bkReal.y, bkReal.x).val[0] = (1 - alpha) * gm.at<Vec3f>(bkReal.y, bkReal.x).val[0] + alpha * curData[j];
						gm.at<Vec3f>(bkReal.y, bkReal.x).val[1] = (1 - alpha) * gm.at<Vec3f>(bkReal.y, bkReal.x).val[1] + alpha * (gm.at<Vec3f>(bkReal.y, bkReal.x).val[0] - curData[j]) * (gm.at<Vec3f>(bkReal.y, bkReal.x).val[0] - curData[j]);
						gm.at<Vec3f>(bkReal.y, bkReal.x).val[2] = 1.f / (float)mask.at<ushort>(bkReal.y, bkReal.x);
						//float temp = gm.at<Vec3f>(bkReal.y, bkReal.x).val[2];
						//cout << "b" << endl;
					}
					dstData[j] = 0;
				}
				else//����ǰ����
				{
					dstData[j] = 255;
					//cout << "q" << endl;
				}
			}
			else //���ڱ�����
			{
				gm.at<Vec3f>(i, j).val[0] = curData[j];
				gm.at<Vec3f>(i, j).val[1] = variance;
				gm.at<Vec3f>(i, j).val[2] = 1.f;
				dstData[j] = 0;
			}
		}
	}
}

void GaussModel::updateMask(Mat H)
{
	updateGray();
	vector<Point> vPtsImg1, vPtsImg2;
	if (ImageOverlap(gm.rows, gm.cols, H, vPtsImg1, vPtsImg2)) //���ص�����
	{
		RotatedRect minRect = minAreaRect(vPtsImg1);
		Point2f vertices[4];
		vector<Point2f> vPts;
		minRect.points(vertices);
		for (size_t i = 0; i < 4; i++)
		{
			line(lap, vertices[i], vertices[(i + 1) % 4], Scalar(0, 0, 255), 1, 8);
			vPts.push_back(vertices[i]);
		}

		for (size_t i = 0; i < mask.rows; i++)
		{
			ushort* maskData = mask.ptr<ushort>(i);
			for (size_t j = 0; j < mask.cols; j++)
			{
				if (pointPolygonTest(vPts, Point2f(j, i), false) > -1)	//���ص������ڲ�
				{
					maskData[j] ++;
				}
				else  //�����ص�����
				{
					maskData[j] = 1;
				}
			}
		}
	}
	//Mat maskForShow;
	//normalize(mask, maskForShow, 0, 255, NORM_MINMAX);
	//cout << mask.at<ushort>(480-1, 0) << endl;
	//cout << gm.at<Vec3f>(480 - 1, 0).val[0] << endl;
	//imshow("mask", maskForShow);
}

void GaussModel::updateGray()
{
	for (size_t i = 0; i < gm.rows; i++)
	{
		Vec3f* gmData = gm.ptr<Vec3f>(i);
		uchar* grayData = gray.ptr<uchar>(i);
		for (size_t j = 0; j < gm.cols; j++)
		{
			//����ת��
			grayData[j] = gmData[j].val[0];
		}
	}
}

//����alpha�仯����
void GaussModel::drawAlpha(Mat& bk, size_t t)
{
	if (t < 1000)
	{
		
	}
	else
	{

	}
}
