//
//    cv_lib_filters:
//       An extended library of opencv gaussian-based filters.
//       contents:
//       1D multi-order gaussian filters (Option: Hilbert Transform)
//       2D multi-order anistropic gaussian filters (Option: Hilbert Transform)
//       2D central-surrouding gaussian filters
//       2D texton filters
//       texton executation
//
//    Created by Di Yang, Vicent Rabaud, and Gary Bradski on 31/05/13.
//    Copyright (c) 2013 The Australian National University. 
//    and Willow Garage inc.
//    All rights reserved.
//    
//

#include "cv_lib_filters.hh"
using namespace std;

namespace libFilters
{ 
  /********************************************************************************
   * Hilbert Transform
   ********************************************************************************/
  void
  convolveDFT(const cv::Mat & inputA,
	      const cv::Mat & inputB,
	      cv::Mat & output,
	      bool label)
  {
    bool flag = label? SAME_SIZE : EXPAND_SIZE;
    cv::Mat TempA, TempB;
    int r=inputA.rows, c=inputA.cols;
    inputA.copyTo(TempA);
    inputB.copyTo(TempB);

    int width = cv::getOptimalDFTSize(inputA.cols+inputB.cols-1);
    cv::copyMakeBorder(TempA, TempA, 0, 0, 0, width-TempA.cols-1, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    cv::copyMakeBorder(TempB, TempB, 0, 0, 0, width-TempB.cols-1, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    cv::dft(TempA, TempA, cv::DFT_ROWS, TempA.rows);
    cv::dft(TempB, TempB, cv::DFT_ROWS, TempB.rows);
    cv::mulSpectrums(TempA, TempB, TempA, cv::DFT_ROWS, false);
    cv::dft(TempA, TempA, cv::DFT_INVERSE+cv::DFT_SCALE, output.rows); 
    
    if(flag){
      int W_o = (TempA.cols-c)/2;
      TempA(cv::Rect(W_o, 0, c, r)).copyTo(output);
    }
    else
      TempA.copyTo(output);
  }

  void
  hilbertTransform1D(const cv::Mat & input,
		     cv::Mat & output,
		     bool label)
  {
    bool flag = label? SAME_SIZE : EXPAND_SIZE;
    cv::Mat temp;
    input.copyTo(temp);
    if(temp.cols != 1 && temp.rows != 1){
      cout<<"Input must be a 1D matrix"<<endl;
    }
    int length = (temp.rows > temp.cols)? temp.rows : temp.cols;
    if(input.cols == 1)
      cv::transpose(temp, temp);
    cv::Mat hilbert(1, length, CV_32FC1);
    int half_len = (length-1)/2;
    for(int i = 0; i < hilbert.cols; i++){
        int m = i-half_len;
	if( m % 2 == 0)
	  hilbert.at<float>(0, i) = 0.0;
	else
	  hilbert.at<float>(0, i) = 1.0/(M_PI*double(m));
    }
    convolveDFT(temp, hilbert, temp, label);
    if(input.cols == 1)
      cv::transpose(temp, temp);
    temp.copyTo(output);
  }

  /********************************************************************************
   * Standard orientation generation
   ********************************************************************************/
  double* 
  standard_filter_orientations(int n_ori,
			       bool label)
  {
    bool flag = label? RAD : DEG;
    double* oris = new double[n_ori];
    double ori = 0.0;
    if(flag){
      double ori_step = (n_ori>0) ? (M_PI/double(n_ori)) : 0;
      for(size_t i=0; i<n_ori; i++, ori += ori_step)
	oris[i] = ori;
    }
    else{
      double ori_step = (n_ori>0) ? (180.0/double(n_ori)) : 0;
      for(size_t i=0; i<n_ori; i++, ori += ori_step)
	oris[i] = ori;
    }
    return oris;
  }

  /********************************************************************************
   * Distribution Normalize and Mean value shifting
   ********************************************************************************/
  void
  normalizeDistr(const cv::Mat & input,
		 cv::Mat & output,
		 bool label)
  {
    bool flag = label ? ZERO : NON_ZERO;
    input.copyTo(output);
    output.convertTo(output, CV_32FC1);
    cv::Mat ones = cv::Mat::ones(output.rows, output.cols, output.type());
    double sumAbs = 0.0;
    double mean = 0.0;
    /* If required, zero-mean shift*/
    if(flag){
      for(size_t i=0; i<output.rows; i++)
	for(size_t j=0; j< output.cols; j++)
	  if(flag)
	    mean += output.at<float>(i,j);
      mean = mean/(double(output.rows*output.cols));
      cv::addWeighted(output, 1.0, ones, -mean, 0.0, output);
    }
    /* Distribution Normalized */
    for(size_t i=0; i<output.rows; i++)
      for(size_t j=0; j< output.cols; j++)
	sumAbs += fabs(output.at<float>(i,j));
    cv::divide(output, ones, output, 1.0/sumAbs);
  }

  /********************************************************************************
   * Matrix Rotation
   ********************************************************************************/
  
  int
  supportRotated(int x,
		  int y,
		  double ori,
		  bool label)
  {
    double sin_ori, cos_ori, mag0, mag1;
    bool flag = label ? X_ORI : Y_ORI;
    if(flag){
      cos_ori = double(x)*cos(ori);
      sin_ori = double(y)*sin(ori);
    }
    else{
      cos_ori = double(y)*cos(ori);
      sin_ori = double(x)*sin(ori);
    }
    mag0 = fabs(cos_ori - sin_ori);
    mag1 = fabs(cos_ori + sin_ori);
    return int(((mag0 > mag1)? mag0 : mag1)+1.0);
  }
  
  void 
  rotate_2D_crop(const cv::Mat & input,
		 cv::Mat & output,
		 double ori,
		 int len_cols,
		 int len_rows,
		 bool label)
  {
    bool flag = label? RAD:DEG;
    cv::Mat tmp;
    cv::Mat rotate_M = cv::Mat::zeros(2, 3, CV_32FC1);
    cv::Point center = cv::Point((input.cols-1)/2, (input.rows-1)/2);
    double angle;
    
    if(flag)
      angle = ori/M_PI*180.0;
    else
      angle = ori;

    rotate_M = cv::getRotationMatrix2D(center, angle, 1.0);
    
    /* Apply rotation transformation to a matrix */
    cv::warpAffine(input, tmp, rotate_M, input.size(), cv::INTER_LINEAR);
    
    /* Cropping */
    int border_rows = (input.rows - len_rows)/2;
    int border_cols = (input.cols - len_cols)/2;
    cv::Rect cROI(border_cols, border_rows, len_cols, len_rows);
    output = tmp(cROI);
  }

  void rotate_2D(const cv::Mat & input,
		 cv::Mat & output,
		 double ori,
		 bool label)
  {
    rotate_2D_crop(input, output, ori, input.cols, input.rows, label);
  }

 /********************************************************************************
   * Filters Generation
   ********************************************************************************/
  
  /* 1D multi-order gaussian filter generation */
  void 
  gaussianFilter1D(int half_len,
		    double sigma,
		    int deriv,
		    bool label,
		    cv::Mat & output)
  {
    bool hlbrt = label? HILBRT_ON : HILBRT_OFF; 
    int len = 2*half_len+1;
    cv::Mat ones = cv::Mat::ones(len, 1, CV_32F);
    double sum_abs;
    output  = cv::getGaussianKernel(len, sigma, CV_32F);
    if(deriv == 1){
      for(int i=0; i<len; i++){
	output.at<float>(i) = output.at<float>(i)*double(half_len-i);
      }
    }
    else if(deriv == 2){
      for(int i=0; i<len; i++){
	double x = double(i-half_len);
	output.at<float>(i) = output.at<float>(i)*(x*x/sigma-1.0); 
      }
    }
    if(hlbrt)
      hilbertTransform1D(output, output, SAME_SIZE);

    if(deriv > 0)
      normalizeDistr(output, output, ZERO);
    else
      normalizeDistr(output, output, NON_ZERO);
  }

  void 
  gaussianFilter1D(double sigma,
		   int deriv,
		   bool hlbrt,
		   cv::Mat & output)
  {
    int half_len = int(sigma*3.0);
    gaussianFilter1D(half_len, sigma, deriv, hlbrt, output);
  }

  /* multi-order anistropic gaussian filter generation */
  void
  gaussianFilter2D(int half_len,
		    double ori,
		    double sigma_x,
		    double sigma_y,
		    int deriv,
		    bool hlbrt,
		    cv::Mat & output)
  {
    /* rotate support ROI */
    int len = 2*half_len+1;
    int half_len_rotate_x = supportRotated(half_len, half_len, ori, X_ORI);
    int half_len_rotate_y = supportRotated(half_len, half_len, ori, Y_ORI);
    int half_rotate_len = (half_len_rotate_x > half_len_rotate_y)? half_len_rotate_x : half_len_rotate_y;
    int len_rotate= 2*half_rotate_len+1;    
    cv::Mat output_x, output_y;

    /*   Conduct Compution */    
    gaussianFilter1D(half_rotate_len, sigma_x, 0,     HILBRT_OFF, output_x);
    gaussianFilter1D(half_rotate_len, sigma_y, deriv, hlbrt, output_y);
    output = output_x*output_y.t();
    rotate_2D_crop(output, output, ori, len, len, DEG);
    
    /*  Normalize  */
    if(deriv > 0)
      normalizeDistr(output, output, ZERO);
    else
      normalizeDistr(output, output, NON_ZERO);
  }


  void 
  gaussianFilter2D(double ori,
		   double sigma_x,
		   double sigma_y,
		   int deriv,
		   bool hlbrt,
		   cv::Mat & output)
  {
    bool flag = hlbrt? HILBRT_ON : HILBRT_OFF; 
    /* actual size of kernel */
    int half_len_x = int(sigma_x*3.0);
    int half_len_y = int(sigma_y*3.0);
    int half_len = (half_len_x>half_len_y)? half_len_x : half_len_y;
    gaussianFilter2D(half_len, ori, sigma_x, sigma_y, deriv, hlbrt, output);
  }

  /* Central-surrounding gaussian filter */
  void
  gaussianFilter2D_cs(int half_len,
		       double sigma_x,
		       double sigma_y,
		       double scale_factor,
		       cv::Mat & output)
  {
    double sigma_x_c = sigma_x/scale_factor;
    double sigma_y_c = sigma_y/scale_factor;
    cv::Mat output_cen, output_sur;
    gaussianFilter2D(half_len, 0.0, sigma_x_c, sigma_y_c, 0, HILBRT_OFF, output_cen);
    gaussianFilter2D(half_len, 0.0, sigma_x,   sigma_y,   0, HILBRT_OFF, output_sur);
    cv::addWeighted(output_sur, 1.0, output_cen, -1.0, 0.0, output);
    normalizeDistr(output, output, ZERO);
  }

  void
  gaussianFilter2D_cs(double sigma_x,
		      double sigma_y,
		      double scale_factor,
		      cv::Mat & output)
  {
    int half_len_x = int(sigma_x*3.0);
    int half_len_y = int(sigma_y*3.0);
    int half_len = (half_len_x>half_len_y)? half_len_x : half_len_y;    
    gaussianFilter2D_cs(half_len, sigma_x, sigma_y, scale_factor, output);
  }
 
  /* A set of multi-order anistropic gaussian filters generation */
  void
  gaussianFilters(int n_ori,
		  double sigma,
		  int deriv,
		  bool hlbrt,
		  double enlongation,
		  vector<cv::Mat> & filters)
  {
    double sigma_x = sigma;
    double sigma_y = sigma/enlongation;
    double* oris;
    filters.resize(n_ori);
    oris = standard_filter_orientations(n_ori, DEG);
    for(size_t i=0; i<n_ori; i++)
      gaussianFilter2D(oris[i], sigma_x, sigma_y, deriv, hlbrt, filters[i]);
  }

  /* Even or odd gaussian multi-order gaussian filters generation */
  void
  oeFilters(int n_ori,
	     double sigma,
	     vector<cv::Mat> & filters,
	     bool label)
  {
    bool flag = label ? OE_EVEN : OE_ODD;
    if(flag)
      gaussianFilters(n_ori, sigma, 2, HILBRT_OFF, 3.0, filters);
    else
      gaussianFilters(n_ori, sigma, 2, HILBRT_ON, 3.0, filters);
  }

  /* Texton Filters Generation */
  void 
  textonFilters(int n_ori,
		double sigma,
		vector<cv::Mat> & filters)
  {
    vector<cv::Mat> even_filters;
    vector<cv::Mat> odd_filters;
    cv::Mat f_cs;
    filters.resize(2*n_ori+1);
    oeFilters(n_ori, sigma, even_filters, OE_EVEN);
    oeFilters(n_ori, sigma, odd_filters,  OE_ODD );
    gaussianFilter2D_cs(sigma, sigma, M_SQRT2, f_cs);
    
    for(size_t i=0; i<n_ori; i++){
      even_filters[i].copyTo(filters[i]);
      odd_filters[i].copyTo(filters[n_ori+i]);
    }
    f_cs.copyTo(filters[2*n_ori]);
  }

  /********************************************************************************
   * Texton Filters Executation
   ********************************************************************************/

  void
  textonRun(const cv::Mat & input,
	    cv::Mat & output,
	    int n_ori,
	    int Kmean_num,
	    double sigma_sm,
	    double sigma_lg)
  {
    vector<cv::Mat> filters_small, filters_large, filters;
    cv::Mat blur, labels, k_samples;
    
    filters.resize(4*n_ori+2);
    textonFilters(n_ori, sigma_sm, filters_small);
    textonFilters(n_ori, sigma_lg, filters_large);
    
    for(size_t i=0; i<2*n_ori+1; i++){
      filters_small[i].copyTo(filters[i]);
      filters_large[i].copyTo(filters[2*n_ori+1+i]);
    }
    
    k_samples = cv::Mat::zeros(input.rows*input.cols, 4*n_ori+2, CV_32FC1);
    
    for(size_t idx=0; idx< 4*n_ori+2; idx++){
      cv::filter2D(input, blur, CV_32F, filters[idx], cv::Point(-1, -1), 0.0, cv::BORDER_REFLECT);
      for(size_t i = 0; i<k_samples.rows; i++)
	k_samples.at<float>(i, idx) = blur.at<float>(i%blur.rows, i/blur.rows);
    }
    
    cv::kmeans(k_samples, Kmean_num, labels, 
	       cv::TermCriteria(cv::TermCriteria::EPS, 10, 0.0001), 
	       3, cv::KMEANS_PP_CENTERS);
      
    output = cv::Mat::zeros(blur.rows, blur.cols, CV_32SC1);
    for(size_t i=0; i<labels.rows; i++)
      output.at<int>(i%output.rows, i/output.rows)=labels.at<int>(i, 0);
    output.convertTo(output, CV_32FC1);
  }

  cv::Mat 
  weight_matrix_disc(int r) 
  {
    int size = 2*r + 1;
    int r_sq = r*r;
    cv::Mat weights = cv::Mat::zeros(size, size, CV_32SC1);
    for (int i = 0; i< weights.rows; i++)
      for (int j = 0; j< weights.cols; j++) {
	int x_sq = (i-r)*(i-r);
	int y_sq = (j-r)*(j-r);
        if ((x_sq + y_sq) <= r_sq)
	  weights.at<int>(i, j) = 1;
      }
    weights.at<int>(r, r) = 0;
    return weights;
  }

/*
 * Construct orientation slice lookup map.
 */
  cv::Mat 
  orientation_slice_map(int r, 
			int n_ori)
  {
  /* initialize map */
    int size = 2*r+1;
    cv::Mat slice_map = cv::Mat::zeros(size, size, CV_32FC1);
    for (int i = 0, y = size/2; i < size; i++, y--)
      for (int j = 0, x = -size/2; j < size; j++, x++) {
	double ori = atan2(double(y), double(x));
	slice_map.at<float>(i, j) = ori/M_PI*180.0;
      }
    return slice_map;
  }

   
  void
  gradient_hist_2D(const cv::Mat & label,
		   int r,
		   int n_ori,
		   int num_bins,
		   cv::Mat & gaussian_kernel,
		   vector<cv::Mat> & gradients)
  {
    double *oris;
    cv::Mat weights, slice_map, label_exp;
    cv::Mat hist_left  = cv::Mat::zeros(1, num_bins, CV_32FC1);
    cv::Mat hist_right = cv::Mat::zeros(1, num_bins, CV_32FC1);    
    weights = weight_matrix_disc(r);
    slice_map = orientation_slice_map(r, n_ori);
    oris = standard_filter_orientations(n_ori, DEG);
    gradients.resize(n_ori);
    for(size_t i=0; i<n_ori; i++)
      gradients[i] = cv::Mat::zeros(label.rows, label.cols, CV_32FC1);
    cv::copyMakeBorder(label, label_exp, r, r, r, r, cv::BORDER_REFLECT);
    
    for(int i=r; i<label_exp.rows-r; i++)
      for(int j=r; j<label_exp.cols-r; j++)
	for(size_t idx = 0; idx < n_ori; idx++){
	  hist_left.setTo(0.0);
	  hist_right.setTo(0.0);
	  for(int x= -r; x <= r; x++)
	    for(int y= -r; y <= r; y++){
	      int bin = int(label_exp.at<float>(i+x, j+y));
	      if(slice_map.at<float>(x+r, y+r) > oris[idx]-180.0 && 
		 slice_map.at<float>(x+r, y+r) <= oris[idx])
		hist_right.at<float>(0, bin) += double(weights.at<int>(x+r, y+r));
	      else
		hist_left.at<float>(0, bin) += double(weights.at<int>(x+r, y+r));
	    }
	  
	  if(gaussian_kernel.cols == 1)
	    cv::transpose(gaussian_kernel, gaussian_kernel);
	  convolveDFT(hist_right, gaussian_kernel, hist_right, SAME_SIZE);
	  convolveDFT(hist_left, gaussian_kernel, hist_left, SAME_SIZE);
	  
	  double sum_l = 0.0, sum_r =0.0; 
	  for(size_t nn = 0; nn<num_bins; nn++){
	    sum_l += hist_left.at<float>(0, nn);
	    sum_r += hist_right.at<float>(0, nn);
	  }
	  
	  double tmp = 0.0, tmp1 = 0.0, tmp2 = 0.0, hist_r, hist_l;
	  for(size_t nn = 0; nn<num_bins; nn++){
	    if(sum_r == 0)
	      hist_r = hist_right.at<float>(0,nn);
	    else
	      hist_r = hist_right.at<float>(0,nn)/sum_r;
	    
	    if(sum_l == 0)
	      hist_l = hist_left.at<float>(0,nn);
	    else
	      hist_l = hist_left.at<float>(0,nn)/sum_l;

	    tmp1 = hist_r-hist_l;
	    tmp2 = hist_r+hist_l;
	    if(tmp2 < 0.00001)
	      tmp2 = 1.0;

	    tmp += 0.5*(tmp1*tmp1)/tmp2;
	  }
	  gradients[idx].at<float>(i-r,j-r) = tmp;
	} 
  }

  void
  gradient_hist_2D(const cv::Mat & label,
		   int r,
		   int n_ori,
		   int num_bins,
		   vector<cv::Mat> & gradients)
  {
    int length = 7;
    cv::Mat impulse_resp = cv::Mat::zeros(1, length, CV_32FC1);
    impulse_resp.at<float>(0, (length-1)/2) = 1.0;
    gradient_hist_2D(label, r, n_ori, num_bins, impulse_resp, gradients);
  }

  void
  Display_EXP(const vector<cv::Mat> & images, 
	      const char* name)
  {
    int Depth = images.size();
    int sub_c = images[0].cols;
    int sub_r = images[0].rows;
    int w_n = 4;
    int h_n = int(double(Depth)/double(w_n)+0.5);
    cv::Mat dispimage(h_n*sub_r, w_n*sub_c, CV_32FC1);

    int c = 0;
    for(size_t i=0; i<h_n; i++)
      for(size_t j=0; j<w_n; j++){
	for(size_t x=0; x<sub_r; x++)
	  for(size_t y=0; y<sub_c; y++)
	    dispimage.at<float>(i*sub_r+x, j*sub_c+y) = images[c].at<float>(x, y);
	c++;
      }
    imshow(name, dispimage); 
  }

}