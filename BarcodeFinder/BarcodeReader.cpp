#include "BarcodeReader.h"

using namespace cv;
using namespace std;

// ��������� ��� EAN
BarcodeReader::BarcodeReader()
    : LEFT_PATTERNS{
        "0001101", "0011001", "0010011", "0111101", "0100011",
        "0110001", "0101111", "0111011", "0110111", "0001011"
    },
    RIGHT_PATTERNS{
      "1110010", "1100110", "1101100", "1000010", "1011100",
      "1001110", "1010000", "1000100", "1001000", "1110100"
    } {
}

// �������� ������� 
string BarcodeReader::detectAndDecodeBarcode(const string& imagePath) {
    Mat image = imread(imagePath);
    if (image.empty()) {
        cerr << "�� ������� ��������� �����������!" << endl;
        return "";
    }

    // ������������� �����������
    Mat processed = preprocessImage(image);

    // ����������� ���������
    Rect barcodeROI = detectBarcodeRegion(processed);
    if (barcodeROI.width == 0 || barcodeROI.height == 0) {
        cout << "�������� �� ���������!" << endl;
        return "";
    }

    // ��������� � ������������� ���������
    Mat barcodeImage = image(barcodeROI);
    string result = decodeBarcode(barcodeImage);

    // ������������ ����������
    rectangle(image, barcodeROI, Scalar(0, 255, 0), 2);
    putText(image, "Barcode: " + result, Point(barcodeROI.x, barcodeROI.y - 10),
        FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

    imshow("Detected Barcode", image);
    waitKey(0);

    return result;
}

Mat BarcodeReader::preprocessImage(const Mat& image) {
    Mat gray, blurred, gradX, binary;

    // ����������� � grayscale
    cvtColor(image, gray, COLOR_BGR2GRAY);

    // ���������� ����
    GaussianBlur(gray, blurred, Size(3, 3), 0);

    // ��������� �� X 
    Sobel(blurred, gradX, CV_32F, 1, 0, -1);
    convertScaleAbs(gradX, gradX);

    // �����������
    threshold(gradX, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // ��������������� �������� ��� ���������� �����
    Mat kernel = getStructuringElement(MORPH_RECT, Size(21, 7));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel);

    return binary;
}

Rect BarcodeReader::detectBarcodeRegion(const Mat& binary) {
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;

    findContours(binary, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // ����������
    sort(contours.begin(), contours.end(),
        [](const vector<Point>& c1, const vector<Point>& c2) {
            return contourArea(c1) > contourArea(c2);
        });

    // ����� �������
    for (const auto& contour : contours) {
        Rect rect = boundingRect(contour);
        double aspectRatio = (double)rect.width / rect.height;

        if (aspectRatio > 1 && aspectRatio < 2 && rect.area() > 1000 && rect.area() < 100000) {
            return rect;
        }
    }

    return Rect();
}

string BarcodeReader::decodeBarcode(const Mat& barcodeROI) {
    Mat gray, binary;
    cvtColor(barcodeROI, gray, COLOR_BGR2GRAY);

    // �����������
    adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C,
        THRESH_BINARY, 11, 2);

    // ������� �������������
    vector<int> profile = getIntensityProfile(binary);

    // ������������ �������
    vector<int> binaryProfile = normalizeProfile(profile);

    // ������� ������ �� 1 �� 10 ��������
    for (int width = 1; width <= 10; width++) {
        cout << "������� ������������� ��� ������ " << width << " ��������..." << endl;

        // EAN-13
        string result = decodeEAN13WithWidth(binaryProfile, width);
        if (result.find("������") == string::npos) {
            cout << "�����! ������� ������: " << width << " �������� (EAN-13)" << endl;
            return result;
        }

        // EAN-8
        result = decodeEAN8(binaryProfile, width);
        if (result.find("������") == string::npos) {
            cout << "�����! ������� ������: " << width << " �������� (EAN-8)" << endl;
            return result;
        }
    }

    return "�� ������� ������������";
}

vector<int> BarcodeReader::getIntensityProfile(const Mat& binary) {
    int middleRow = binary.rows / 2;
    vector<int> profile(binary.cols);

    for (int col = 0; col < binary.cols; col++) {
        int sum = 0;
        // ���������� �� ���������� �������
        for (int row = middleRow - 2; row <= middleRow + 2; row++) {
            if (row >= 0 && row < binary.rows) {
                sum += binary.at<uchar>(row, col);
            }
        }
        profile[col] = sum / 5;
    }

    return profile;
}

vector<int> BarcodeReader::normalizeProfile(const vector<int>& profile) {
    vector<int> binaryProfile;
    int threshold = 128;

    for (int value : profile) {
        binaryProfile.push_back(value > threshold ? 0 : 1);
    }

    return binaryProfile;
}

string BarcodeReader::decodeEAN13WithWidth(const vector<int>& profile, int width) {
    string result;

    int start = findStartMarkerWithWidth(profile, width);
    if (start == -1) {
        cout << "  ��������� ������ �� ������ ��� ������ " << width << "px" << endl;
        return "������: ��������� ������ �� ������";
    }

    cout << "  ������������� ��� ������ " << width << "px, �����: " << start << endl;

    // ������������� 6 ����� ����
    for (int i = 0; i < 6; i++) {
        int digitStart = start + 3 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, LEFT_PATTERNS, width);
        if (digit == -1) {
            cout << "  ������ ������������� ����� ����� " << i << " ��� ������ " << width << "px" << endl;
            return "������ ������������� ����� ����� " + to_string(i);
        }
        result += to_string(digit);
    }

    int centerStart = findCenterMarkerWithWidth(profile, start + 3 * width + 6 * 7 * width, width);
    if (centerStart == -1) {
        cout << "  ����������� ������ �� ������ ��� ������ " << width << "px" << endl;
        return "������: ����������� ������ �� ������";
    }

    // ������������� 6 ������ ����
    for (int i = 0; i < 6; i++) {
        int digitStart = centerStart + 5 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, RIGHT_PATTERNS, width);
        if (digit == -1) {
            cout << "  ������ ������������� ������ ����� " << i << " ��� ������ " << width << "px" << endl;
            return "������ ������������� ������ ����� " + to_string(i);
        }
        result += to_string(digit);
    }

    cout << "  ������� ����������� ��������: " << result << " ��� ������ " << width << "px" << endl;
    return result;
}

int BarcodeReader::findStartMarkerWithWidth(const vector<int>& profile, int width) {
    // ��� ������ 1 �������
    if (width == 1) {
        for (size_t i = 0; i < profile.size() - 2; i++) {
            if (profile[i] == 1 && profile[i + 1] == 0 && profile[i + 2] == 1) {
                return i;
            }
        }
    }
    // ��� ������ ������ 1 �������
    else {
        for (size_t i = 0; i < profile.size() - 2 * width; i += width) {
            if (profile[i] == 1 && profile[i + width] == 0 && profile[i + 2 * width] == 1) {
                return i;
            }
        }
    }
    return -1;
}

int BarcodeReader::findCenterMarkerWithWidth(const vector<int>& profile, int start, int width) {
    // ��� ������ 1 �������
    if (width == 1) {
        for (int i = start; i < (int)profile.size() - 4; i++) {
            if (profile[i] == 0 && profile[i + 1] == 1 && profile[i + 2] == 0 &&
                profile[i + 3] == 1 && profile[i + 4] == 0) {
                return i;
            }
        }
    }
    // ��� ������ ������ 1 �������
    else {
        for (int i = start; i < (int)profile.size() - 4 * width; i += width) {
            if (profile[i] == 0 && profile[i + width] == 1 && profile[i + 2 * width] == 0 &&
                profile[i + 3 * width] == 1 && profile[i + 4 * width] == 0) {
                return i;
            }
        }
    }
    return -1;
}

int BarcodeReader::decodeDigitWithWidth(const vector<int>& profile, int start,
    const vector<string>& patterns, int width) {
    string pattern;

    // ���������� 7-������� �������� � ������ ������
    for (int i = 0; i < 7; i++) {
        int pos = start + i * width;
        if (pos < (int)profile.size()) {
            pattern += to_string(profile[pos]);
        }
        else {
            return -1;
        }
    }

    cout << "  ������� ��� ������ " << width << "px: " << pattern << endl;

    // ����� ���������� ��������
    for (int i = 0; i < 10; i++) {
        if (pattern == patterns[i]) {
            cout << "  ������� �����: " << i << endl;
            return i;
        }
    }

    // ����� � ��������
    for (int i = 0; i < 10; i++) {
        if (hammingDistance(pattern, patterns[i]) <= 1) {
            cout << "  ������� ����� � ��������: " << i << endl;
            return i;
        }
    }

    return -1;
}

int BarcodeReader::hammingDistance(const string& s1, const string& s2) {
    if (s1.length() != s2.length()) return 10;
    int distance = 0;
    for (size_t i = 0; i < s1.length(); i++) {
        if (s1[i] != s2[i]) distance++;
    }
    return distance;
}

string BarcodeReader::decodeEAN8(const vector<int>& profile, int width) {
    string result;

    int start = findStartMarkerWithWidth(profile, width);
    if (start == -1) return "������: ��������� ������ �� ������";

    cout << "  ������������� EAN-8 ��� ������ " << width << "px, �����: " << start << endl;

    // ������������� 4 ����� ����
    for (int i = 0; i < 4; i++) {
        int digitStart = start + 3 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, LEFT_PATTERNS, width);
        if (digit == -1) {
            cout << "  ������ ������������� ����� ����� " << i << " ��� ������ " << width << "px" << endl;
            return "������ ������������� ����� ����� " + to_string(i);
        }
        result += to_string(digit);
    }

    int centerStart = findCenterMarkerWithWidth(profile, start + 3 * width + 4 * 7 * width, width);
    if (centerStart == -1) {
        cout << "  ����������� ������ �� ������ ��� ������ " << width << "px" << endl;
        return "������: ����������� ������ �� ������";
    }

    // ������������� 4 ������ ����
    for (int i = 0; i < 4; i++) {
        int digitStart = centerStart + 5 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, RIGHT_PATTERNS, width);
        if (digit == -1) {
            cout << "  ������ ������������� ������ ����� " << i << " ��� ������ " << width << "px" << endl;
            return "������ ������������� ������ ����� " + to_string(i);
        }
        result += to_string(digit);
    }

    cout << "  ������� ����������� EAN-8: " << result << " ��� ������ " << width << "px" << endl;
    return result;
}