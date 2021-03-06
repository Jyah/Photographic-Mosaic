﻿// Project: Photographic Mosaic
#include "main.h"

std::mutex mtx;
#define MAX_THREAD 16
struct thread_args {
	int count, px_c, rows, cols;
	Mat* image;
	vector<string>* img_crop_list;
};
int step_i = 0;
int row = 800, col = 1500; //screen resolution
int size_a = 256; //size of replacement square sub-image

Mat canvas;
void similarity(Mat imgA, Mat imgB, float& emd);

void pixwise(Mat imgA, Mat imgB, float& emd){
	emd = 0;
	for(int i = 0;i<imgA.rows;i++){
		for(int j =0;j<imgA.cols;j++){
			Vec3b vA = imgA.at<Vec3b>(i,j);
			Vec3b vB = imgB.at<Vec3b>(i,j);
			for(int k=0;k<imgA.dims;k++){
				float dv = (vA.val[k]-vB.val[k])/255.0;// 255.0 to make it float
				emd = emd + pow(dv,2);
			}
		}
	}
	return;
};

void resize(const Mat sub_img, Mat &img_crop, int rows, int cols) {
	img_crop = Mat::zeros(rows, cols, CV_8UC3);
	int scale = sub_img.rows / img_crop.rows;
	for (int i = 0; i < img_crop.rows; i++) {
		for (int j = 0; j < img_crop.cols; j++) {
			img_crop.at<Vec3b>(i, j) = sub_img.at<Vec3b>(i * scale, j * scale);
		}
	}
	return;
};

void* multi(void* arg) {
	struct thread_args* args = (struct thread_args*)arg;
	int count = args->count;
	int px_c = args->px_c;
	int rows = args->rows;
	int cols = args->cols;
	Mat image = *(args->image);
	vector<string> img_crop_list = *(args->img_crop_list);
	int idx = 0, Tp_x, Tp_y;
	float emd, min_emd;//emd 0 is best matching
	Mat img_crops, sub_image, aux;
	int core = step_i++;
	int py_r = px_c;
	string filepath;
	int N_r = floor(image.rows / py_r) - 1;
	int N_c = floor(image.cols / px_c) - 1;
	float width_factor = (canvas.cols - canvas.cols % col) / col + 1;
	float height_factor = (canvas.rows - canvas.rows % row) / row + 1;
	float factor;
	(width_factor > height_factor) ? (factor = width_factor) : (factor = height_factor);
	int width = canvas.cols / factor;
	int height = canvas.rows / factor;
	// each thread computes 1/MAX_THREAD of the matrix
	Mat img_crops_mp;
	for (int c = core * N_c*N_r/(MAX_THREAD-1); c < (core + 1) * N_c*N_r/(MAX_THREAD-1); c++) {
		if (c < N_r * N_c) {
			int i = (c - c % N_c) / N_c;
			int j = c % N_c;
			min_emd = 1<<20;
			Tp_x = j * size_a;
			Tp_y = i * size_a;
			sub_image = image(Rect(j * px_c, i * py_r, px_c, py_r));
			for (int k = 0; k < count; k++) {
				filepath = img_crop_list[k];
				img_crops = imread(filepath, IMREAD_COLOR);// read image file
				resize(img_crops, img_crops_mp, rows, cols);
				//similarity(sub_image, img_crops, emd);
				pixwise(sub_image, img_crops_mp, emd);
				if (emd < min_emd) {
					min_emd = emd;
					idx = k;
				}
			}
			// add cropped image to make new image
			img_crops = imread(img_crop_list[idx], IMREAD_COLOR);
			mtx.lock();
			aux = canvas.colRange(Tp_x, Tp_x + size_a).rowRange(Tp_y, Tp_y + size_a);
			img_crops.copyTo(aux);
			// update new image
			namedWindow("Display frame", WINDOW_NORMAL);
			resizeWindow("Display frame", width, height);
			imshow("Display frame", canvas);
			waitKey(1);			
			mtx.unlock();

		}
	}
}

void meanIntensity(Mat M, double& blue, double& green, double& red);
void linspace(double start, double end, int N, double vec[]);
int minDist(Mat bgr_list, double blue, double green, double red, vector<string> path_list);
void makeList(bool yn, string filename, string path);
void makeTestData(bool yn, char im_dir[], int rows, int cols, int ncolors);
Mat readList(char bgrfile[], vector<string>& path_list);



// examples: make && ./Mosaic main_image pix_c source_lib target_lib 0
int main(int argc, char* argv[]) {
	int py_r, px_c,N_r,N_c;
	py_r = px_c = atoi(argv[2]);
	int rows = atoi(argv[2]);//pixels in row of subimage
	int cols = rows;//pixels in column of subimage

	// Prepare the image data sets
	vector<int> compression_params;
	compression_params.push_back(IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(9);
	string imgsets = argv[3];
	char* imgcrops = argv[4];
	string filepath, filename;
	double scale;
	Mat image, img_crop;
	//create resized image pool
	int count = 0;
	vector<string> img_crop_list;
	char buf[100];
	Mat sub_img;
	int rescale = true;
	for (const auto& entry : fs::directory_iterator(imgsets)) {
		const auto filenameStr = entry.path().filename().string();
		filepath = imgsets + filenameStr;
		image = imread(filepath, IMREAD_COLOR);// read image file
		if (image.rows > image.cols) {
				transpose(image, image);
		}
		filename = imgcrops + filenameStr;
		img_crop_list.push_back(filename);
		if (rescale) {
			scale = min(floor(image.rows / size_a), floor(image.cols / size_a));
			sub_img = image.colRange(0, scale * size_a).rowRange(0, scale * size_a);
			resize(sub_img, img_crop, size_a, size_a);
			imwrite(filename, img_crop, compression_params);
		}
		printf("Counting: %d\n", count);
		count++;
	}
	//Read Original Image
	string target = argv[1];
	double blue = 0, green = 0, red = 0;
	image = imread(target, IMREAD_COLOR);// read image file
	N_r = floor(image.rows / py_r)-1;
	N_c = floor(image.cols / px_c)-1;
	// Create a blank New Image
	canvas = Mat::zeros(N_r * size_a, N_c * size_a, CV_8UC3);

	// declaring threads	
	pthread_t threads[MAX_THREAD];
	// prepare argument
	struct thread_args* args = (struct thread_args*)malloc(sizeof(struct thread_args));
	args->count = count;
	args->px_c = px_c;
	args->rows = rows;
	args->cols = cols;
	args->img_crop_list = &img_crop_list;
	args->image = &image;

	// creating threads, each evaluating its own part
	for (int i = 0; i < MAX_THREAD; i++) {
		pthread_create(&threads[i], NULL, multi, (void*)args);
	}
	// joining and waiting for all threads to complete
	for (int i = 0; i < MAX_THREAD; i++) {
		pthread_join(threads[i], NULL);
	}

	// Write Mosaic
	imwrite("Mosaic.png", canvas, compression_params);
	printf("Finished!\n");
	float width_factor = (canvas.cols - canvas.cols % col) / col + 1;
	float height_factor = (canvas.rows - canvas.rows % row) / row + 1;
	float factor;
	(width_factor > height_factor) ? factor = width_factor : factor = height_factor;
	int width = canvas.cols / factor;
	int height = canvas.rows / factor;
	namedWindow("Display frame", WINDOW_NORMAL);
	resizeWindow("Display frame", width, height);
	imshow("Display frame", canvas);
	waitKey(0);
	return 0;
}


// Functions
void similarity(Mat imgA, Mat imgB, float& emd) {
	//variables preparing      
	int hbins = 30, sbins = 32;
	int channels[] = { 0,  1 };
	int histSize[] = { hbins, sbins };
	float hranges[] = { 0, 180 };
	float sranges[] = { 0, 255 };
	const float* ranges[] = { hranges, sranges };

	Mat patch_HSV;
	MatND HistA, HistB;

	//cal histogram & normalization   
	cvtColor(imgA, patch_HSV, COLOR_BGR2HSV);
	calcHist(&patch_HSV, 1, channels, Mat(), // do not use mask   
		HistA, 2, histSize, ranges,
		true, // the histogram is uniform   
		false);
	normalize(HistA, HistA, 0, 1, CV_MINMAX);

	cvtColor(imgB, patch_HSV, COLOR_BGR2HSV);
	calcHist(&patch_HSV, 1, channels, Mat(),// do not use mask   
		HistB, 2, histSize, ranges,
		true, // the histogram is uniform   
		false);
	normalize(HistB, HistB, 0, 1, CV_MINMAX);

	//compare histogram      
	int numrows = hbins * sbins;

	//make signature
	Mat sig1(numrows, 3, CV_32FC1);
	Mat sig2(numrows, 3, CV_32FC1);

	//fill value into signature
	float binval;
	for (int h = 0; h < hbins; h++)
	{
		for (int s = 0; s < sbins; s++)
		{
			binval = HistA.at< float>(h, s);
			sig1.at< float>(h * sbins + s, 0) = binval;
			sig1.at< float>(h * sbins + s, 1) = h;
			sig1.at< float>(h * sbins + s, 2) = s;

			binval = HistB.at< float>(h, s);
			sig2.at< float>(h * sbins + s, 0) = binval;
			sig2.at< float>(h * sbins + s, 1) = h;
			sig2.at< float>(h * sbins + s, 2) = s;
		}
	}

	//compare similarity of 2images using emd.
	emd = cv::EMD(sig1, sig2, DIST_L2); //emd 0 is best matching. 
	return;
}

Mat readList(char bgrfile[], vector<string>& path_list) {
	FILE* ptr = fopen(bgrfile, "r");
	char buf[100];
	double bl, gr, re;

	int lSize = 0;
	char ch;
	while ((ch = fgetc(ptr)) != EOF) {
		lSize++;
	}
	rewind(ptr);
	Mat bgr_list(lSize, 3, CV_64F);
	int i = 0;
	while ((ch = fgetc(ptr)) != EOF) {
		ungetc(ch, ptr);
		fscanf(ptr, "%s %lf %lf %lf\n", buf, &bl, &gr, &re);
		path_list.push_back(buf);
		bgr_list.at<double>(i, 0) = bl;
		bgr_list.at<double>(i, 1) = gr;
		bgr_list.at<double>(i, 2) = re;
		i++;
	}
	fclose(ptr);
	return bgr_list;
}

void makeTestData(bool yn, char im_dir[],int rows, int cols, int ncolors) {
	if (yn) {
		int nb, ng, nr; //number of color in a channel
		nb = ng = nr = ncolors;
		double v[nb];
		linspace(0, 255.0, nb, v);
		Mat im;
		vector<int> compression_params;
		compression_params.push_back(IMWRITE_PNG_COMPRESSION);
		compression_params.push_back(9);
		char buffer[50];
		for (int i = 0; i < nb; i++) {
			Mat ch_B = Mat::ones(rows, cols, CV_8U) * v[i];
			for (int j = 0; j < ng; j++) {
				Mat ch_G = Mat::ones(rows, cols, CV_8U) * v[j];
				for (int k = 0; k < nr; k++) {
					Mat ch_R = Mat::ones(rows, cols, CV_8U) * v[k];
					vector<Mat> channels{ ch_B,ch_G,ch_R };
					merge(channels, im);
					sprintf(buffer, "%s/img_%d_%d_%d.png", im_dir, i, j, k);
					imwrite(buffer, im, compression_params);
				}
			}
		}
	}
	return;
}
void makeList(bool yn,string filename,string path) {
	if (yn) {
		Mat image;
		string filepath;
		double blue = 0, green = 0, red = 0;
		ofstream txtout;
		txtout.open(filename, ios::out | ios::trunc);
		for (const auto& entry : fs::directory_iterator(path)) {
			const auto filenameStr = entry.path().filename().string();
			filepath = path + filenameStr;
			image = imread(filepath, IMREAD_COLOR);// read image file
			meanIntensity(image, blue, green, red);
			txtout << filepath << "\t" << blue << "\t" << green << "\t" << red << endl;
		}
		txtout.close();
	}
	return;
}
int minDist(Mat bgr_list, double blue, double green, double red, vector<string> path_list) {
	double mindist = 1e10;
	double dist = 0;
	int idx = 0;
	for (int i = 0; i < bgr_list.rows; i++) {
		dist = pow(bgr_list.at<double>(i, 0) - blue, 2) + pow(bgr_list.at<double>(i, 1) - green, 2) + pow(bgr_list.at<double>(i, 2) - red, 2);
		if (dist < mindist) {
			mindist = dist;
			idx = i;
		}
	}
	return idx;
}
void meanIntensity(Mat M, double& blue, double& green, double& red) {
	blue = green = red = 0;
	for (int i = 0; i < M.rows; i++)
	{
		for (int j = 0; j < M.cols; j++)
		{
			Vec3b intensity = M.at<Vec3b>(i, j);//imread gets uchar
			blue += intensity.val[0];
			green += intensity.val[1];
			red += intensity.val[2];
		}
	}
	int N = M.rows * M.cols;
	blue /= N;
	green /= N;
	red /= N;
	return;
}
void linspace(double start, double end, int N, double vec[]) {
	double step = (end - start) / (N - 1);
	for (int i = 0; i < N; i++) {
		vec[i] = step * i;
	}
	return;
}