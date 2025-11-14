#ifndef BARCODEREADER_H
#define BARCODEREADER_H

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

class BarcodeReader {
private:
    const std::vector<std::string> LEFT_PATTERNS;
    const std::vector<std::string> RIGHT_PATTERNS;
    const std::vector<std::string> G_PATTERNS;

public:
    BarcodeReader();
    std::string detectAndDecodeBarcode(const std::string& imagePath);

private:
    cv::Mat preprocessImage(const cv::Mat& image);
    cv::Rect detectBarcodeRegion(const cv::Mat& binary);
    std::string decodeBarcode(const cv::Mat& barcodeROI);
    std::vector<int> getIntensityProfile(const cv::Mat& binary);
    std::vector<int> normalizeProfile(const std::vector<int>& profile);
    std::string decodeEAN13WithWidth(const std::vector<int>& profile, int width);
    std::string decodeEAN8(const std::vector<int>& profile, int width);
    std::vector<bool> determineEncodingScheme(const std::vector<int>& profile, int start, int width, int& firstDigit);
    int findStartMarkerWithWidth(const std::vector<int>& profile, int width);
    int findCenterMarkerWithWidth(const std::vector<int>& profile, int start, int width);
    int decodeDigitWithWidth(const std::vector<int>& profile, int start,
        const std::vector<std::string>& patterns, int width);
};

#endif