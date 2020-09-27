#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <string_view>
#include <iostream>
#include <fstream> 
#include <sstream>
#include <cstdlib>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <algorithm> 
#include <chrono>
#include <utility>
#include <limits>

#include "gmcp.hpp"
#include "utils.hpp"

#include <opencv2/opencv.hpp>


void print_usage_info()
{
    const char* usage_info =
    "\n    -s, --segment_size   frames in a segment\n"
    "    -i, --in_video         input video path\n"
    "    -o, --out_video        output video path\n"
    "    -d, --detector         object detector (ssd or yolo)\n"
    "    -c, --detector_cfg     path to detector cfg\n"
    "    -f, --tmp_folder       path to folder where temporary files will be stored\n"
    "    -h, --help             show this help msg";
    std::cout << usage_info << std::endl;
}

void get_parameters(int argc, char **argv, int& segment_size, std::string& in_video, std::string& out_video, 
                    std::string& detector, std::string& detector_cfg, std::string& tmp_folder)
{
    int opt;
    while((opt = getopt(argc, argv, "s:i:o:d:c:f:h")) != -1)
    {
        switch (opt)
        {
            case 's':
                segment_size = std::stoi(optarg);
                break;
            case 'i':
                in_video = optarg;
                break;
            case 'o':
                out_video = optarg;
                break;
            case 'd':
                detector = optarg;
                break;
            case 'c':
                detector_cfg = optarg;
                break;
            case 'f':
                tmp_folder = optarg;
                break;
            case 'h':
                print_usage_info();
                exit(0);
            default:
                std::cout << "Unsupported parameter passed to the script. Aborting." << std::endl;
                print_usage_info();
                abort();
        }
    }
}

void verify_parameters(int segment_size, std::string in_video, std::string out_video, 
                       std::string detector, std::string detector_cfg, std::string tmp_folder)
{
    if (segment_size == 0)
    {
        std::cout << "Please specify segment size. Aborting.";
        exit(0);
    }
    if (in_video == "")
    {
        std::cout << "Please specify input video path. Aborting.";
        exit(0);
    }
    if (out_video == "")
    {
        std::cout << "Please specify output video path. Aborting.";
        exit(0);
    }
    if (detector != "yolo" && detector != "ssd")
    {
        std::cout << "Unsupported detector. Aborting.";
        exit(0);
    }
    if (detector_cfg == "")
    {
        std::cout << "Please specify detector config path. Aborting.";
        exit(0);
    }
    if (tmp_folder == "")
    {
        std::cout << "Please specify path where tmp files should be stored. Aborting.";
        exit(0);
    }
}

void print_parameters(int segment_size, std::string in_video, std::string out_video, 
                      std::string detector, std::string detector_cfg, std::string tmp_folder)
{
    printf("Segment size set to %d\n", segment_size);
    printf("Input video path set to %s\n", in_video.c_str());
    printf("Output video path set to %s\n", out_video.c_str());
    printf("Detector set to %s\n", detector.c_str());
    printf("Detector cfg path set to %s\n", detector_cfg.c_str());
    printf("Temporary files will be stored in %s\n", tmp_folder.c_str());
}

void print_exec_time(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end)
{
    std::cout << "Exec time = " << std::chrono::duration_cast<std::chrono::minutes>(end - begin).count() 
            << "[min] (" << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count()  
            << "[s])" << std::endl;
}

int get_video_capture_frame_cnt(const cv::VideoCapture& cap)
{
    return cap.get(cv::CAP_PROP_FRAME_COUNT);
}

int get_trimmed_frame_cnt(const cv::VideoCapture& cap, int frames_in_segment) 
{
    int video_in_frame_cnt = get_video_capture_frame_cnt(cap);
    std::cout << "Input video frames count: " << video_in_frame_cnt << std::endl;
    return video_in_frame_cnt / frames_in_segment * frames_in_segment;
}

std::string get_frame_path(int frame, std::string tmp_folder)
{
    std::stringstream ss;
    ss << tmp_folder << "/img/frame" << std::to_string(frame) << ".jpeg";
    std::string path = ss.str();
    return path;
}

void trim_video(std::string video_in, std::string video_out, int frame_cnt) 
{
    std::stringstream ss;
    ss << "ffmpeg -i " << video_in << " -vframes " << std::to_string(frame_cnt) 
       << " -acodec copy -vcodec copy " << video_out << " -y";
    std::string trim_command = ss.str();
    std::cout << "Executing: " << trim_command << std::endl;
    system(trim_command.c_str());
}

void detect(std::string detector, std::string detector_cfg, int frame_cnt, std::string video, std::string tmp_folder) 
{
    // initiates object detections on selected frames of the trimmed video
    // the detections are then stored in csv files
    std::stringstream ss;
    ss << "python3 ../detectors/detect.py --detector " << detector 
       << " --cfg " << detector_cfg
       << " --video " << video
       << " --tmp_folder " << tmp_folder
       << " --frame_cnt " << std::to_string(frame_cnt);
    std::string detect_command = ss.str();
    std::cout << "Executing: " << detect_command << std::endl;
    system(detect_command.c_str());
}

std::vector<BoundingBox> load_frame_detections(std::string csv_file) 
{
    std::vector<BoundingBox> boxes;
    std::ifstream f(csv_file);
    std::string line, colname;
    int val;

    if(!f.is_open()) 
         throw std::runtime_error("Could not open file");

    while(std::getline(f, line))
    {
        std::stringstream ss(line); 
        int coords[4];
        constexpr int const coord_cnt = 4; 
        for (int i = 0; i < coord_cnt; i++)
        {
            std::string substr;
            getline(ss, substr, ',');
            coords[i] = std::stoi(substr);
        }
        BoundingBox box = {
            .x_min = coords[0],
            .y_min = coords[1],
            .x_max = coords[2],
            .y_max = coords[3]
        };
        boxes.push_back(box);
    }
    f.close();
    return boxes;
}

std::vector<std::vector<BoundingBox>> load_detections(int frame_cnt, std::string tmp_folder) 
{
    std::vector<std::vector<BoundingBox>> detections(frame_cnt, std::vector<BoundingBox>());
    for (int i = 0; i < frame_cnt; i += 1) 
    {
        std::stringstream ss;
        ss << tmp_folder << "/csv/frame" << i << ".csv";
        std::string csv_path = ss.str();
        detections[i] = load_frame_detections(csv_path);
    }
    return detections;
}

unsigned int get_max_detections_per_frame(const std::vector<std::vector<BoundingBox>> &detections)
{
    unsigned int maxi = 0;
    for(auto const& d : detections)
        if (d.size() > maxi)
            maxi = d.size();
    std::cout << "Max number of detections per frame is: " << maxi << std::endl;
    return maxi;
}

auto get_detection_centers_and_histograms(const std::vector<std::vector<BoundingBox>> &detections,
                                          int frame_cnt, int segment_cnt, int segment_size,
                                          const std::string &tmp_folder)
{
    std::vector<std::vector<Location>> centers(frame_cnt, std::vector<Location>());
    std::vector<std::vector<cv::Mat>> histograms(frame_cnt, std::vector<cv::Mat>());
    constexpr int channels[3] = {0, 1, 2};
    constexpr float range[2] = {0, 256};
    const float * ranges[3] = {range, range, range};
    constexpr int histSize[3] = {8, 8, 8};
    for (int i = 0; i < segment_cnt; i++) 
    {
        int start_frame = segment_size * i;
        for (int j = start_frame; j < start_frame + segment_size; j++)
        {
            cv::Mat frame = cv::imread(get_frame_path(j, tmp_folder));
            for(auto const& d: detections[j])
            {
                int width = d.x_max - d.x_min;
                int height = d.y_max - d.y_min;
                int x_center = (d.x_min + d.x_max) / 2;
                int y_center = (d.y_min + d.y_max) / 2;
                Location loc = {
                    .x = x_center,
                    .y = y_center
                };
                centers[j].push_back(loc);
                
                cv::Mat hist;
                cv::Mat detection = frame(cv::Rect(x_center, y_center, width, height));
                cv::calcHist(&detection, 1, channels, cv::Mat(), hist, 3, histSize, ranges);
                histograms[j].push_back(hist);
            }
        }
    }
    return std::make_pair(centers, histograms);
}

auto get_net_cost(int frame_cnt, const std::vector<std::vector<cv::Mat>> &histograms)
{  
    using namespace std;
    vector<vector<vector<HistInterKernel>>> net_cost(frame_cnt, 
                                                     vector<vector<HistInterKernel>>(frame_cnt, 
                                                                                     vector<HistInterKernel>()));
    for (int i = 0; i < frame_cnt; i++)
        for (int k = 0; k < histograms[i].size(); k++)
            for (int j = 0; j < frame_cnt; j++)
                for (int l = 0; l < histograms[j].size(); l++)
                {
                    double histogram_intersection_kernel = cv::compareHist(histograms[i][k], 
                                                                        histograms[j][l], 
                                                                        3); // CV_COMP_INTERSECT
                    HistInterKernel hik = {
                        .detection_idx1 = k,
                        .detection_idx2 = l,
                        .value = histogram_intersection_kernel
                    };
                    net_cost[i][j].push_back(hik);
                }
    return net_cost;
}

void prepare_tmp_video(const cv::VideoCapture& in_cap, int desired_frame_cnt, 
                       std::string tmp_folder, std::string in_video, std::string tmp_video)
{
    const int video_in_frame_cnt = get_video_capture_frame_cnt(in_cap);
    if (video_in_frame_cnt != desired_frame_cnt)
    {
        std::string const trimmed_video = tmp_folder + "/trim.mp4";
        trim_video(in_video, trimmed_video, desired_frame_cnt);
        std::cout << "Input video frames count cut to: " << desired_frame_cnt << std::endl;
        mv(trimmed_video, tmp_video);
    }
    else
    {
        cp(in_video, tmp_video);
    }
}

HistInterKernel get_cheapest(const std::vector<HistInterKernel> &hiks)
{
    double maxi = std::numeric_limits<double>::max();
    HistInterKernel mini = {
        .detection_idx1 = -1,
        .detection_idx2 = -1,
        .value = maxi
    };
    for(auto hik: hiks)
    {
        if (hik.value < mini.value)
        {
            mini = hik;
        }
    }
    return mini;
}

HistInterKernel get_cheapest(const std::vector<HistInterKernel> &hiks, int detection_idx1)
{
    double maxi = std::numeric_limits<double>::max();
    HistInterKernel mini = {
        .detection_idx1 = -1,
        .detection_idx2 = -1,
        .value = maxi
    };
    for(auto hik: hiks)
    {   
        if (hik.detection_idx1 == detection_idx1 && hik.value < mini.value)
        {
            mini = hik;
        }
    }
    return mini;
}

std::vector<int> get_initial_detection_path(const std::vector<std::vector<std::vector<HistInterKernel>>> &net_cost,
                                              int segment_size)
{
    // returns indices of detections in subsequent frames that form a brute-force solution
    std::vector<int> detection_idxs;
    detection_idxs.reserve(segment_size);

    auto first_hik = get_cheapest(net_cost[0][1]);
    detection_idxs.push_back(first_hik.detection_idx1);
    detection_idxs.push_back(first_hik.detection_idx2);

    for (int i = 1; i < segment_size - 1; i++)
    {
        auto hik = get_cheapest(net_cost[i][i+1], detection_idxs.back());
        detection_idxs.push_back(hik.detection_idx2);
    }
    return detection_idxs;
}

auto calculate_appearance_cost(const std::vector<std::vector<std::vector<HistInterKernel>>> &net_cost,
                               const std::vector<int> detection_indices)
{
    
}

int main(int argc, char **argv) {
    auto begin = std::chrono::steady_clock::now();
    int segment_size = 0;
    std::string in_video, out_video, detector, detector_cfg, tmp_folder;
    
    get_parameters(argc, argv, segment_size, in_video, out_video, detector, detector_cfg, tmp_folder);
    verify_parameters(segment_size, in_video, out_video, detector, detector_cfg, tmp_folder);
    print_parameters(segment_size, in_video, out_video, detector, detector_cfg, tmp_folder);
    make_tmp_dirs(tmp_folder);

    cv::VideoCapture in_cap(in_video);
    const int frame_cnt = get_trimmed_frame_cnt(in_cap, segment_size);
    const int segment_cnt = frame_cnt / segment_size;
    const std::string tmp_video = tmp_folder + "/input.mp4";
    prepare_tmp_video(in_cap, frame_cnt, tmp_folder, in_video, tmp_video);

    detect(detector, detector_cfg, frame_cnt - 1, tmp_video, tmp_folder);
    auto detections = load_detections(frame_cnt, tmp_folder);
    auto max_detections_per_frame = get_max_detections_per_frame(detections);
    auto colors = get_colors(max_detections_per_frame);
    
    auto cah = get_detection_centers_and_histograms(detections, 
                                                    frame_cnt, 
                                                    segment_cnt, segment_size,
                                                    tmp_folder);
    auto centers = cah.first;
    auto histograms = cah.second;
    auto net_cost = get_net_cost(frame_cnt, histograms);

    // for (int i = 0; i < segment_cnt; i++)
    // {
    //     int j = 0;
    //     while (j < max_detections_per_frame)
    //     {
    //         // once a pedestrian is tracked, take corresponding detections out of net_cost matrix
            
    //         j++;
    //         // remove_done_frames
    //     }
    // }

    auto detection_idxs = get_initial_detection_path(net_cost, segment_size);

    clear_tmp(tmp_folder);
    auto end = std::chrono::steady_clock::now();
    print_exec_time(begin, end);
    return 0;
}






// auto calculate_motion_cost(const std::vector<std::vector<Location>> &centers)
//{
/*
function sol_cost=calc_sol_cost_motion(sol,centers_mat,n_frames_orig)

[n_cands n_frames]=size(centers_mat);     to zwróci max detections x frame_cnt

sub_inds=sub2ind(size(centers_mat),sol-(0:(n_frames-1))*n_cands,1:n_frames);
centers_mat_sols=centers_mat(sub_inds);
centers1=cell2mat(centers_mat_sols(:));

velocity_vecs=diff(centers1);
    
% motion_model_type,'Holistic_Spatial')
    
[sol_cost,~] = calc_sol_cost_motion_Spatial_holistic_amirs(sol,centers_mat);
alpha=0.000; % the balance between appearance and motion. bigger alpha, more effect from motion
%alpha=0.002; % the balance between appearance and motion. bigger alpha, more effect from motion

sol_cost=sol_cost*n_frames*n_frames*alpha;
    
% end of holistic spatial
end
*/
//}

// auto initialize_solution(const vector<vector<vector<HistInterKernel>>> &net_cost, 
//                          const std::vector<std::vector<Location>> &centers, 
//                          int max_detections_per_frame, int frame_cnt)
//{

/*
function [INIT,init_sol_cost]=initialize_sol(netcostmat,NN,n_clusters,centers_mat,n_frames_orig)
`
sum1=sum(netcostmat,2);    co to jest za suma

sum2=reshape(sum1,[NN n_clusters]);   pionowy wektor przerabiamy na macierz 2dd

[~, mI]=min(sum2);    biorę indeksy tych detekcji, dla których wagi były najmniejsze

IPI=0:NN:((n_clusters-1)*NN);  to jest wektor od 0 do (n_clusters-1)*NN co NN

INIT=IPI+mI;    dodawanie wektorów elementwise

init_sol_cost=calc_sol_cost_appearance(INIT,netcostmat)+calc_sol_cost_motion(INIT,centers_mat,n_frames_orig);

end
    */
    // powinno zwrócić std::pair init, init sol cost
    // powinno pobrać: netcostmat,NN,n_clusters,centers_mat,n_frames_orig,motion_model_type
//     return ;
// }
