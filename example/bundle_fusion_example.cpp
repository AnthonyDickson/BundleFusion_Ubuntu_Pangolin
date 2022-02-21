#include <iostream>
#include <string>
#include <fstream>
#include <BundleFusion.h>
#include <ConditionManager.h>
#include <dirent.h>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <GlobalAppState.h>
#include <unistd.h>
#include <sys/time.h>
#include <json.hpp>

std::string get_working_path() {
    char temp[PATH_MAX];
    return (getcwd(temp, sizeof(temp)) ? std::string(temp) : std::string(""));
}

std::vector<std::string> getFileNames(const std::string &folderName) {
    struct dirent *ptr;
    DIR *dir;
    dir = opendir(folderName.c_str());
    std::vector<std::string> filenames;

    while ((ptr = readdir(dir)) != nullptr) {
        if (ptr->d_name[0] == '.') {
            continue;
        }

        filenames.emplace_back(ptr->d_name);
    }

    std::sort(filenames.begin(), filenames.end());

    return filenames;
}

vec4f quaternionFromHomogenousTransform(const mat4f &M) {
    float m00 = M.at(0, 0);
    float m01 = M.at(0, 1);
    float m02 = M.at(0, 2);
    float m10 = M.at(1, 0);
    float m11 = M.at(1, 1);
    float m12 = M.at(1, 2);
    float m20 = M.at(2, 0);
    float m21 = M.at(2, 1);
    float m22 = M.at(2, 2);

    float tr = m00 + m11 + m22;
    float qx, qy, qz, qw;

    if (tr > 0) {
        float S = sqrt(tr + 1.0f) * 2; // S=4*qw
        qx = (m21 - m12) / S;
        qy = (m02 - m20) / S;
        qz = (m10 - m01) / S;
        qw = 0.25f * S;
    } else if ((m00 > m11)&(m00 > m22)) {
        float S = sqrt(1.0f + m00 - m11 - m22) * 2; // S=4*qx
        qx = 0.25f * S;
        qy = (m01 + m10) / S;
        qz = (m02 + m20) / S;
        qw = (m21 - m12) / S;
    } else if (m11 > m22) {
        float S = sqrt(1.0f + m11 - m00 - m22) * 2; // S=4*qy
        qx = (m01 + m10) / S;
        qy = 0.25f * S;
        qz = (m12 + m21) / S;
        qw = (m02 - m20) / S;
    } else {
        float S = sqrt(1.0f + m22 - m00 - m11) * 2; // S=4*qz
        qx = (m02 + m20) / S;
        qy = (m12 + m21) / S;
        qz = 0.25f * S;
        qw = (m10 - m01) / S;
    }

    return {qx, qy, qz, qw};
}

void saveTrajectory() {
    std::vector<mat4f> trajectory;
    getTrajectoryManager()->getOptimizedTransforms(trajectory);
    std::cout << "Got trajectory of length " << trajectory.size() <<  std::endl;

    const auto trajectoryOutputPath = GlobalAppState::get().s_generateMeshDir + "/trajectory.txt";
    std::ofstream trajectoryFile;
    trajectoryFile.open(trajectoryOutputPath);

    if (trajectoryFile.is_open()) {
        std::cout << "Writing camera trajectory data to " << trajectoryOutputPath << std::endl;

        for (const auto &pose: trajectory) {
            const auto rot = quaternionFromHomogenousTransform(pose);
            const auto pos = pose.getTranslation();

            trajectoryFile << rot.x << " " << rot.y << " " << rot.z << " " << rot.w << " " << pos.x << " " << pos.y << " " << pos.z << std::endl;
        }

        trajectoryFile.close();
    } else {
        std::cerr << "Could not open trajectory output file " << trajectoryOutputPath << " for writing to." << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cout
                << "usage: ./bundle_fusion_example /path/to/zParametersDefault.txt /path/to/zParametersBundlingDefault.txt /path/to/dataset"
                << std::endl;
        return -1;
    }

    std::cout << "======================BundleFusion Example=========================" << std::endl;
    std::cout << "==  " << std::endl;
    std::cout << "==  This is an example usage for BundleFusion SDK interface" << std::endl;
    std::cout << "==  Author: FangGet" << std::endl;
    std::cout << "==  " << std::endl;
    std::cout << "===================================================================" << std::endl;

    auto app_config_file = std::string(argv[1]);
    auto bundle_config_file = std::string(argv[2]);
    const auto datasetFolder = std::string(argv[3]);
    const auto rgbFolder = datasetFolder + "/rgb";
    const auto depthFolder = datasetFolder + "/masked_depth";

    std::cout << "Working Directory: " << get_working_path() << std::endl;
//
//    for (const auto &filename : getFileNames("../")) {
//        std::cout << filename << std::endl;
//    }

    const auto metadata_path = datasetFolder + "/metadata.json";
    std::ifstream metadata_json{metadata_path};

    if (!metadata_json.good()) {
        std::cerr << "Could not open the file " << metadata_path << std::endl;
        return -1;
    } else {
        std::cout << "Found metadata file at " << metadata_path << std::endl;
    }

    nlohmann::json metadata;
    metadata_json >> metadata;

    const float depth_scale = metadata["depth_scale"];
    std::cout << "Depth Scale: " << depth_scale << std::endl;

    if (!std::ifstream(app_config_file).good()) {
        std::cerr << "Could not open the config file at " << app_config_file << std::endl;
        return -1;
    }
    if (!std::ifstream(bundle_config_file).good()) {
        std::cerr << "Could not open the config file at " << bundle_config_file << std::endl;
        return -1;
    }

    int cudaRuntimeVersion = 0;
    cudaRuntimeGetVersion((int *) &cudaRuntimeVersion);
    std::cout << "CUDA Runtime Version: " << cudaRuntimeVersion << std::endl;

    int cudaDriverVersion = 0;
    cudaDriverGetVersion((int *) &cudaDriverVersion);
    std::cout << "CUDA Driver Version: " << cudaDriverVersion << std::endl;

    int numDevices = 0;
    cudaGetDeviceCount((int *) &numDevices);
    std::cout << "Found " << numDevices << " GPUs." << std::endl;

    // step 1: init all resources
    if (!initBundleFusion(app_config_file, bundle_config_file)) {
        std::cerr << "BundleFusion init failed, exit." << std::endl;
        return -1;
    } else {
        std::cout << "BundleFusion initialised successfully." << std::endl;
    }

    // read for bundlefusion dataset from http://graphics.stanford.edu/projects/bundlefusion/
    auto rgbFilenames = getFileNames(rgbFolder);
    std::cout << "Found " << rgbFilenames.size() << " RGB frames." << std::endl;

    auto depthFilenames = getFileNames(depthFolder);
    std::cout << "Found " << depthFilenames.size() << " depth maps." << std::endl;

    if (rgbFilenames.size() != depthFilenames.size()) {
        std::cerr << "Uneven number of files found in RGB folder and depth folder (" << rgbFilenames.size() << " vs "
                  << depthFilenames.size() << ")." << std::endl;
        return -1;
    }

    for (unsigned int i = 0; i < rgbFilenames.size(); i++) {
        auto frameNum = i + 1;
        std::cout << "processing frame " << frameNum << "..." << std::endl;

        std::string rgb_path = rgbFolder + "/" + rgbFilenames[i];
        std::string dep_path = depthFolder + "/" + depthFilenames[i];
        //std::string pos_path = data_root + "/" + filename + ".pose.txt";
        cv::Mat rgbImage = cv::imread(rgb_path);

        cv::Mat depthImage;
        cv::imread(dep_path, cv::IMREAD_UNCHANGED).convertTo(depthImage, CV_32FC1);
        depthImage = 1000.f * depth_scale * depthImage; // Assuming VTM dataset, this line will convert depth to mm.
        depthImage.convertTo(depthImage, CV_16UC1); // This is the datatype that the de

        if (rgbImage.empty() || depthImage.empty()) {
            std::cout << "no image founded" << std::endl;
        }

//         cv::imshow ( "rgb_image", rgbImage );
//         cv::imshow ( "depth_image", depthImage );
//         char c = cv::waitKey ( 20 );


        if (processInputRGBDFrame(rgbImage, depthImage)) {
            std::cout << "\tSuccess! frame " << frameNum << " added into BundleFusion." << std::endl;
        } else {
            std::cout << "\tFailed! frame " << frameNum << " not added into BundleFusion." << std::endl;
        }

        if (ConditionManager::shouldExit()) {
            std::cout << "Exiting early." << std::endl;
            break;
        }
    }

    saveTrajectory();

    //    while(cv::waitKey (20) != 'q');
    auto meshPath = GlobalAppState::get().s_generateMeshDir + "/mesh.ply";
    std::cout << "Saving mesh to " << meshPath << std::endl;
    saveMeshIntoFile(meshPath, true);

    deinitBundleFusion();

    std::cout << "Done!" << std::endl;

    return 0;
}
