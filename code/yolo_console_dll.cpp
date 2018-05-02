#include <iostream>
#include <iomanip> 
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable

#ifdef _WIN32
#define OPENCV
#endif

// To use tracking - uncomment the following line. Tracking is supported only by OpenCV 3.x
//#define TRACK_OPTFLOW

#include "yolo_v2_class.hpp"	// imported functions from DLL

#ifdef OPENCV
#include <opencv2/opencv.hpp>			// C++
#include "opencv2/core/version.hpp"
#ifndef CV_VERSION_EPOCH
#include "opencv2/videoio/videoio.hpp"
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_MAJOR)""CVAUX_STR(CV_VERSION_MINOR)""CVAUX_STR(CV_VERSION_REVISION)
#pragma comment(lib, "opencv_world" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_cudaoptflow" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_cudaimgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_core" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_imgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_highgui" OPENCV_VERSION ".lib")
#else
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_EPOCH)""CVAUX_STR(CV_VERSION_MAJOR)""CVAUX_STR(CV_VERSION_MINOR)
#pragma comment(lib, "opencv_core" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_imgproc" OPENCV_VERSION ".lib")
#pragma comment(lib, "opencv_highgui" OPENCV_VERSION ".lib")
#endif


cv::Scalar obj_id_to_color(int obj_id) {
	int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };
	int const offset = obj_id * 123457 % 6;
	int const color_scale = 150 + (obj_id * 123457) % 100;
	cv::Scalar color(colors[offset][0], colors[offset][1], colors[offset][2]);
	color *= color_scale;
	return color;
}

class preview_boxes_t {
	enum { frames_history = 30 };	// how long to keep the history saved

	struct preview_box_track_t {
		unsigned int track_id, obj_id, last_showed_frames_ago;
		bool current_detection;
		cv::Mat mat_obj;
		preview_box_track_t() : track_id(0), obj_id(0), last_showed_frames_ago(frames_history), current_detection(false){}
	};
	std::vector<preview_box_track_t> preview_box_track_id;
	size_t const preview_box_size, bottom_offset;
	bool const one_off_detections;
public:
	preview_boxes_t(size_t _preview_box_size = 100, size_t _bottom_offset = 100, bool _one_off_detections = false) :
		preview_box_size(_preview_box_size), bottom_offset(_bottom_offset), one_off_detections(_one_off_detections)
	{}

	//void draw_preview_boxes(cv::Mat src_mat, cv::Mat draw_mat, std::vector<bbox_t> result_vec, bool draw_boxes = true)
	void set(cv::Mat src_mat, std::vector<bbox_t> result_vec)
	{
		size_t const count_preview_boxes = src_mat.cols / preview_box_size;
		if (preview_box_track_id.size() != count_preview_boxes) preview_box_track_id.resize(count_preview_boxes);

		// increment frames history
		for (auto &i : preview_box_track_id)
			i.last_showed_frames_ago = std::min((unsigned)frames_history, i.last_showed_frames_ago + 1);

		// occupy empty boxes
		for (auto &k : result_vec) {
			bool found = false;
			for (auto &i : preview_box_track_id) {
				if (i.track_id == k.track_id) {
					if (!one_off_detections)
						i.last_showed_frames_ago = 0;
					found = true;
					break;
				}
			}
			if (!found) {
				for (auto &i : preview_box_track_id) {
					if (i.last_showed_frames_ago == frames_history) {
						if (!one_off_detections && k.frames_counter == 0) break;
						i.track_id = k.track_id;
						i.obj_id = k.obj_id;
						i.last_showed_frames_ago = 0;
						break;
					}
				}
			}
		}

		// draw preview box (from old or current frame)
		for (size_t i = 0; i < preview_box_track_id.size(); ++i)
		{
			// get object image
			cv::Mat dst = preview_box_track_id[i].mat_obj;
			preview_box_track_id[i].current_detection = false;

			for (auto &k : result_vec) {
				if (preview_box_track_id[i].track_id == k.track_id) {
					if (one_off_detections && preview_box_track_id[i].last_showed_frames_ago > 0) break;
					bbox_t b = k;
					cv::Rect r(b.x, b.y, b.w, b.h);
					cv::Rect img_rect(cv::Point2i(0, 0), src_mat.size());
					cv::Rect rect_roi = r & img_rect;
					if (rect_roi.width > 1 || rect_roi.height > 1) {
						cv::Mat roi = src_mat(rect_roi);
						cv::resize(roi, dst, cv::Size(preview_box_size, preview_box_size));
						preview_box_track_id[i].mat_obj = dst.clone();
						preview_box_track_id[i].current_detection = true;
					}
					break;
				}
			}
		}
	}


	void draw(cv::Mat draw_mat)
	{
		// draw preview box (from old or current frame)
		for (size_t i = 0; i < preview_box_track_id.size(); ++i)
		{
			// draw object image
			cv::Mat dst = preview_box_track_id[i].mat_obj;
			if (preview_box_track_id[i].last_showed_frames_ago < frames_history &&
				dst.size() == cv::Size(preview_box_size, preview_box_size))
			{
				cv::Rect dst_rect_roi(cv::Point2i(i * preview_box_size, draw_mat.rows - bottom_offset), dst.size());
				cv::Mat dst_roi = draw_mat(dst_rect_roi);
				dst.copyTo(dst_roi);

				cv::Scalar color = obj_id_to_color(preview_box_track_id[i].obj_id);
				int thickness = (preview_box_track_id[i].current_detection) ? 5 : 1;
				cv::rectangle(draw_mat, dst_rect_roi, color, thickness);
			}
		}
	}
};


void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names, 
	unsigned int wait_msec = 0, std::string win_name = "window name", int current_det_fps = -1, int current_cap_fps = -1)
{
	int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

	for (auto &i : result_vec) {
		cv::Scalar color = obj_id_to_color(i.obj_id);
		cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 5);
		if (obj_names.size() > i.obj_id) {
			std::string obj_name = obj_names[i.obj_id];
			if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);
			cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
			int const max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
			cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 3, 0), std::max((int)i.y - 30, 0)), 
				cv::Point2f(std::min((int)i.x + max_width, mat_img.cols-1), std::min((int)i.y, mat_img.rows-1)), 
				color, CV_FILLED, 8, 0);
			putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 10), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
		}
	}
	if (current_det_fps >= 0 && current_cap_fps >= 0) {
		std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
		putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(50, 255, 0), 2);
	}

	cv::imshow(win_name, mat_img);
	cv::waitKey(wait_msec);
}
#endif	// OPENCV


void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names) {
	for (auto &i : result_vec) {
		if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << " - ";
		std::cout << "obj_id = " << i.obj_id << ",  x = " << i.x << ", y = " << i.y 
			<< ", w = " << i.w << ", h = " << i.h
			<< std::setprecision(3) << ", prob = " << i.prob << std::endl;
	}
}

std::vector<std::string> objects_names_from_file(std::string const filename) {
	std::ifstream file(filename);
	std::vector<std::string> file_lines;
	if (!file.is_open()) return file_lines;
	for(std::string line; getline(file, line);) file_lines.push_back(line);
	std::cout << "object names loaded \n";
	return file_lines;
}


int main(int argc, char *argv[])
{
	std::string  names_file = "data/voc.names";
	std::string  cfg_file = "cfg/yolo-voc.cfg";
	std::string  weights_file = "yolo-voc.weights";
	std::string filename;

	if (argc > 4) {	//voc.names yolo-voc.cfg yolo-voc.weights test.mp4		
		names_file = argv[1];
		cfg_file = argv[2];
		weights_file = argv[3];
		filename = argv[4];
	}
	else if (argc > 1) filename = argv[1];

	Detector detector(cfg_file, weights_file);

	auto obj_names = objects_names_from_file(names_file);
	std::string out_videofile = "result.avi";
	bool const save_output_videofile = false;
#ifdef TRACK_OPTFLOW
	Tracker_optflow tracker_flow;
	detector.wait_stream = true;
#endif

	while (true) 
	{		
		std::cout << "input image or video filename: ";
		if(filename.size() == 0) std::cin >> filename;
		if (filename.size() == 0) break;
		
		try {
#ifdef OPENCV
			preview_boxes_t large_preview(100, 150, false), small_preview(50, 50, true);

			std::string const file_ext = filename.substr(filename.find_last_of(".") + 1);
			std::string const protocol = filename.substr(0, 7);
			if (file_ext == "avi" || file_ext == "mp4" || file_ext == "mjpg" || file_ext == "mov" || 	// video file
				protocol == "rtmp://" || protocol == "rtsp://" || protocol == "http://" || protocol == "https:/")	// video network stream
			{
				cv::Mat cap_frame, cur_frame, det_frame, write_frame;
				std::queue<cv::Mat> track_optflow_queue;
				int passed_flow_frames = 0;
				std::shared_ptr<image_t> det_image;
				std::vector<bbox_t> result_vec, thread_result_vec;
				detector.nms = 0.02;	// comment it - if track_id is not required
				std::atomic<bool> consumed, videowrite_ready;
				consumed = true;
				videowrite_ready = true;
				std::atomic<int> fps_det_counter, fps_cap_counter;
				fps_det_counter = 0;
				fps_cap_counter = 0;
				int current_det_fps = 0, current_cap_fps = 0;
				std::thread t_detect, t_cap, t_videowrite;
				std::mutex mtx;
				std::condition_variable cv_detected, cv_pre_tracked;
				std::chrono::steady_clock::time_point steady_start, steady_end;
				cv::VideoCapture cap(filename); cap >> cur_frame;
				int const video_fps = cap.get(CV_CAP_PROP_FPS);
				cv::Size const frame_size = cur_frame.size();
				cv::VideoWriter output_video;
				if (save_output_videofile)
					output_video.open(out_videofile, CV_FOURCC('D', 'I', 'V', 'X'), std::max(35, video_fps), frame_size, true);

				while (!cur_frame.empty()) 
				{
					// always sync
					if (t_cap.joinable()) {
						t_cap.join();
						++fps_cap_counter;
						cur_frame = cap_frame.clone();
					}
					t_cap = std::thread([&]() { cap >> cap_frame; });

					// swap result bouned-boxes and input-frame
					if(consumed)
					{
						std::unique_lock<std::mutex> lock(mtx);
						det_image = detector.mat_to_image_resize(cur_frame);
						auto old_result_vec = result_vec;
						result_vec = thread_result_vec;
#ifdef TRACK_OPTFLOW
						// track optical flow
						if (track_optflow_queue.size() > 0) {
							auto tmp_result_vec = detector.tracking_id(result_vec, false);
							small_preview.set(track_optflow_queue.front(), tmp_result_vec);

							//std::cout << "\n !!!! all = " << track_optflow_queue.size() << ", cur = " << passed_flow_frames << std::endl;
							tracker_flow.update_tracking_flow(track_optflow_queue.front());

							while (track_optflow_queue.size() > 1) {
								track_optflow_queue.pop();
								result_vec = tracker_flow.tracking_flow(track_optflow_queue.front(), result_vec, true);
							}
							track_optflow_queue.pop();
							passed_flow_frames = 0;
						}
#endif
						result_vec = detector.tracking_id(result_vec);	// comment it - if track_id is not required

						// add old tracked objects
						for (auto &i : old_result_vec) {
							auto it = std::find_if(result_vec.begin(), result_vec.end(),
								[&i](bbox_t const& b) { return b.track_id == i.track_id && b.obj_id == i.obj_id; });
							bool track_id_absent = (it == result_vec.end());
							if (track_id_absent) {
								if (i.frames_counter-- > 1)
									result_vec.push_back(i);
							}
							else
								it->frames_counter = std::min((unsigned)3, i.frames_counter + 1);
						}

						consumed = false;
						cv_pre_tracked.notify_all();
					}
					// launch thread once - Detection
					if (!t_detect.joinable()) {
						t_detect = std::thread([&]() {
							auto current_image = det_image;
							consumed = true;
							while (current_image.use_count() > 0) {
								auto result = detector.detect_resized(*current_image, frame_size, 0.20, false);	// true
								++fps_det_counter;
								std::unique_lock<std::mutex> lock(mtx);
								thread_result_vec = result;
								consumed = true;
								cv_detected.notify_all();
								if (detector.wait_stream) {
									while (consumed) cv_pre_tracked.wait(lock);
								}
								current_image = det_image;
							}
						});
					}

					if (!cur_frame.empty()) {
						steady_end = std::chrono::steady_clock::now();
						if (std::chrono::duration<double>(steady_end - steady_start).count() >= 1) {
							current_det_fps = fps_det_counter;
							current_cap_fps = fps_cap_counter;
							steady_start = steady_end;
							fps_det_counter = 0;
							fps_cap_counter = 0;
						}

						large_preview.set(cur_frame, result_vec);
#ifdef TRACK_OPTFLOW
						++passed_flow_frames;
						track_optflow_queue.push(cur_frame.clone());
						result_vec = tracker_flow.tracking_flow(cur_frame, result_vec, true);	// track optical flow
						small_preview.draw(cur_frame);
#endif						
						large_preview.draw(cur_frame);

						draw_boxes(cur_frame, result_vec, obj_names, 3, "window name", current_det_fps, current_cap_fps);	// 3 or 16ms
						//show_console_result(result_vec, obj_names);

						if (output_video.isOpened() && videowrite_ready) {
							if (t_videowrite.joinable()) t_videowrite.join();
							write_frame = cur_frame.clone();
							videowrite_ready = false;
							t_videowrite = std::thread([&]() { 
								 output_video << write_frame; videowrite_ready = true;
							});
						}
					}

#ifndef TRACK_OPTFLOW
					// wait detection result for video-file only (not for net-cam)
					if (protocol != "rtsp://" && protocol != "http://" && protocol != "https:/") {
						std::unique_lock<std::mutex> lock(mtx);
						while (!consumed) cv_detected.wait(lock);
					}
#endif
				}
				if (t_cap.joinable()) t_cap.join();
				if (t_detect.joinable()) t_detect.join();
				if (t_videowrite.joinable()) t_videowrite.join();
				std::cout << "Video ended \n";
			}
			else if (file_ext == "txt") {	// list of image files
				std::ifstream file(filename);
				if (!file.is_open()) std::cout << "File not found! \n";
				else 
					for (std::string line; file >> line;) {
						std::cout << line << std::endl;
						cv::Mat mat_img = cv::imread(line);
						std::vector<bbox_t> result_vec = detector.detect(mat_img);
						show_console_result(result_vec, obj_names);
						//draw_boxes(mat_img, result_vec, obj_names);
						//cv::imwrite("res_" + line, mat_img);
					}
				
			}
			else {	// image file
				cv::Mat mat_img = cv::imread(filename);
				std::vector<bbox_t> result_vec = detector.detect(mat_img);
				result_vec = detector.tracking_id(result_vec);	// comment it - if track_id is not required
				draw_boxes(mat_img, result_vec, obj_names);
				show_console_result(result_vec, obj_names);
			}
#else
			//std::vector<bbox_t> result_vec = detector.detect(filename);

			auto img = detector.load_image(filename);
			std::vector<bbox_t> result_vec = detector.detect(img);
			detector.free_image(img);
			show_console_result(result_vec, obj_names);
#endif			
		}
		catch (std::exception &e) { std::cerr << "exception: " << e.what() << "\n"; getchar(); }
		catch (...) { std::cerr << "unknown exception \n"; getchar(); }
		filename.clear();
	}

	return 0;
}