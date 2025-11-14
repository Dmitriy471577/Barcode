#include "BarcodeReader.h"

using namespace cv;
using namespace std;

// Константы для EAN
BarcodeReader::BarcodeReader()
    : LEFT_PATTERNS{
        "0001101", "0011001", "0010011", "0111101", "0100011",
        "0110001", "0101111", "0111011", "0110111", "0001011"
    },
    RIGHT_PATTERNS{
      "1110010", "1100110", "1101100", "1000010", "1011100",
      "1001110", "1010000", "1000100", "1001000", "1110100"
    },
    G_PATTERNS{
      "0100111", "0110011", "0011011", "0100001", "0011101",
      "0111001", "0000101", "0010001", "0001001", "0010111"
    } {
}

// Основная функция 
string BarcodeReader::detectAndDecodeBarcode(const string& imagePath) {
    Mat image = imread(imagePath);
    if (image.empty()) {
        cerr << "Не удалось загрузить изображение!" << endl;
        return "";
    }

    // Предобработка изображения
    Mat processed = preprocessImage(image);

    // Обнаружение штрихкода
    Rect barcodeROI = detectBarcodeRegion(processed);
    if (barcodeROI.width == 0 || barcodeROI.height == 0) {
        cout << "Штрихкод не обнаружен!" << endl;
        return "";
    }

    // Выделение и декодирование штрихкода
    Mat barcodeImage = image(barcodeROI);
    string result = decodeBarcode(barcodeImage);

    // Визуализация результата
    rectangle(image, barcodeROI, Scalar(0, 255, 0), 2);
    putText(image, "Barcode: " + result, Point(barcodeROI.x, barcodeROI.y - 10),
        FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

    imshow("Detected Barcode", image);
    waitKey(0);

    return result;
}

Mat BarcodeReader::preprocessImage(const Mat& image) {
    Mat gray, blurred, gradX, binary;

    // Конвертация в grayscale
    cvtColor(image, gray, COLOR_BGR2GRAY);

    // Уменьшения шума
    GaussianBlur(gray, blurred, Size(3, 3), 0);

    // Градиента по X 
    Sobel(blurred, gradX, CV_32F, 1, 0, -1);
    convertScaleAbs(gradX, gradX);

    // Бинаризация
    threshold(gradX, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // Морфологические операции для соединения линий
    Mat kernel = getStructuringElement(MORPH_RECT, Size(21, 7));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel);

    return binary;
}

Rect BarcodeReader::detectBarcodeRegion(const Mat& binary) {
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;

    findContours(binary, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Сортировка
    sort(contours.begin(), contours.end(),
        [](const vector<Point>& c1, const vector<Point>& c2) {
            return contourArea(c1) > contourArea(c2);
        });

    // Поиск контура
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

    // Бинаризация
    adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C,
        THRESH_BINARY, 11, 2);

    // Профиль интенсивности
    vector<int> profile = getIntensityProfile(binary);

    // Нормализация профиля
    vector<int> binaryProfile = normalizeProfile(profile);

    // Перебор ширины от 1 до 10 пикселей
    for (int width = 1; width <= 10; width++) {
        cout << "Попытка декодирования для ширины " << width << " пикселей..." << endl;

        // EAN-13
        string result = decodeEAN13WithWidth(binaryProfile, width);
        if (result.find("Ошибка") == string::npos) {
            cout << "УСПЕХ! Найдена ширина: " << width << " пикселей (EAN-13)" << endl;
            return result;
        }

        // EAN-8
        result = decodeEAN8(binaryProfile, width);
        if (result.find("Ошибка") == string::npos) {
            cout << "УСПЕХ! Найдена ширина: " << width << " пикселей (EAN-8)" << endl;
            return result;
        }
    }

    return "Не удалось декодировать";
}

vector<int> BarcodeReader::getIntensityProfile(const Mat& binary) {
    int middleRow = binary.rows / 2;
    vector<int> profile(binary.cols);

    for (int col = 0; col < binary.cols; col++) {
        int sum = 0;
        // Усреднение по нескольким строкам
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
        cout << "  Стартовый маркер не найден для ширины " << width << "px" << endl;
        return "Ошибка: стартовый маркер не найден";
    }

    cout << "  Декодирование для ширины " << width << "px, старт: " << start << endl;

    // Определение схемы кодирования
    int firstDigit;
    vector<bool> encodingScheme = determineEncodingScheme(profile, start, width, firstDigit);
    if (firstDigit == -1) {
        cout << "  Не удалось определить первую цифру" << endl;
        return "Ошибка: не удалось определить первую цифру";
    }

    result += to_string(firstDigit);

    // Декодирование 6 левых цифр
    for (int i = 0; i < 6; i++) {
        int digitStart = start + 3 * width + i * 7 * width;
        int digit;

        if (encodingScheme[i]) {
            // Используем G-паттерны
            digit = decodeDigitWithWidth(profile, digitStart, G_PATTERNS, width);
        }
        else {
            // Используем L-паттерны
            digit = decodeDigitWithWidth(profile, digitStart, LEFT_PATTERNS, width);
        }

        if (digit == -1) {
            cout << "  Ошибка декодирования левой цифры " << i << " для ширины " << width << "px" << endl;
            return "Ошибка декодирования левой цифры " + to_string(i);
        }
        result += to_string(digit);
    }

    int centerStart = findCenterMarkerWithWidth(profile, start + 3 * width + 6 * 7 * width, width);
    if (centerStart == -1) {
        cout << "  Центральный маркер не найден для ширины " << width << "px" << endl;
        return "Ошибка: центральный маркер не найден";
    }

    // Декодирование 6 правых цифр (всегда R-паттерны)
    for (int i = 0; i < 6; i++) {
        int digitStart = centerStart + 5 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, RIGHT_PATTERNS, width);
        if (digit == -1) {
            cout << "  Ошибка декодирования правой цифры " << i << " для ширины " << width << "px" << endl;
            return "Ошибка декодирования правой цифры " + to_string(i);
        }
        result += to_string(digit);
    }

    cout << "  Успешно декодирован штрихкод: " << result << " для ширины " << width << "px" << endl;
    return result;
}

vector<bool> BarcodeReader::determineEncodingScheme(const vector<int>& profile, int start, int width, int& firstDigit) {
    vector<bool> scheme(6, false);
    firstDigit = -1;

    // Декодируем первые 6 цифр используя L и G
    vector<int> lDigits, gDigits;

    for (int i = 0; i < 6; i++) {
        int digitStart = start + 3 * width + i * 7 * width;

        int lDigit = decodeDigitWithWidth(profile, digitStart, LEFT_PATTERNS, width);
        int gDigit = decodeDigitWithWidth(profile, digitStart, G_PATTERNS, width);

        lDigits.push_back(lDigit);
        gDigits.push_back(gDigit);
    }

    // Определяем схему кодирования
    vector<vector<bool>> schemes = {
        {false, false, false, false, false, false}, // 0: LLLLLL
        {false, false, true,  false, true,  true},  // 1: LLGLGG
        {false, false, true,  true,  false, true},  // 2: LLGGLG
        {false, false, true,  true,  true,  false}, // 3: LLGGGL
        {false, true,  false, false, true,  true},  // 4: LGLLGG
        {false, true,  true,  false, false, true},  // 5: LGGLLG
        {false, true,  true,  true,  false, false}, // 6: LGGGLL
        {false, true,  false, true,  false, true},  // 7: LGLGLG
        {false, true,  false, true,  true,  false}, // 8: LGLGGL
        {false, true,  true,  false, true,  false}  // 9: LGGLGL
    };

    for (int digit = 0; digit < 10; digit++) {
        bool valid = true;
        vector<bool> currentScheme = schemes[digit];

        for (int i = 0; i < 6; i++) {
            int expectedDigit;
            if (currentScheme[i]) {
                expectedDigit = gDigits[i];
            }
            else {
                expectedDigit = lDigits[i];
            }

            if (expectedDigit == -1) {
                valid = false;
                break;
            }
        }

        if (valid) {
            firstDigit = digit;
            scheme = currentScheme;
            cout << "  Определена первая цифра: " << digit << " со схемой кодирования" << endl;
            break;
        }
    }

    return scheme;
}

int BarcodeReader::findStartMarkerWithWidth(const vector<int>& profile, int width) {
    // Для ширины 1 пиксель
    if (width == 1) {
        for (size_t i = 0; i < profile.size() - 2; i++) {
            if (profile[i] == 1 && profile[i + 1] == 0 && profile[i + 2] == 1) {
                return i;
            }
        }
    }
    // Для ширины больше 1 пикселя
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
    // Для ширины 1 пиксель
    if (width == 1) {
        for (int i = start; i < (int)profile.size() - 4; i++) {
            if (profile[i] == 0 && profile[i + 1] == 1 && profile[i + 2] == 0 &&
                profile[i + 3] == 1 && profile[i + 4] == 0) {
                return i;
            }
        }
    }
    // Для ширины больше 1 пикселя
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

    // Извлечение 7-битного паттерна с учетом ширины
    for (int i = 0; i < 7; i++) {
        int pos = start + i * width;
        if (pos < (int)profile.size()) {
            pattern += to_string(profile[pos]);
        }
        else {
            return -1;
        }
    }

    cout << "  Паттерн для ширины " << width << "px: " << pattern << endl;

    // Поиск совпадений паттерна
    for (int i = 0; i < 10; i++) {
        if (pattern == patterns[i]) {
            cout << "  Найдена цифра: " << i << endl;
            return i;
        }
    }

    return -1;
}


string BarcodeReader::decodeEAN8(const vector<int>& profile, int width) {
    string result;

    int start = findStartMarkerWithWidth(profile, width);
    if (start == -1) return "Ошибка: стартовый маркер не найден";

    cout << "  Декодирование EAN-8 для ширины " << width << "px, старт: " << start << endl;

    // Декодирование 4 левых цифр
    for (int i = 0; i < 4; i++) {
        int digitStart = start + 3 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, LEFT_PATTERNS, width);
        if (digit == -1) {
            cout << "  Ошибка декодирования левой цифры " << i << " для ширины " << width << "px" << endl;
            return "Ошибка декодирования левой цифры " + to_string(i);
        }
        result += to_string(digit);
    }

    int centerStart = findCenterMarkerWithWidth(profile, start + 3 * width + 4 * 7 * width, width);
    if (centerStart == -1) {
        cout << "  Центральный маркер не найден для ширины " << width << "px" << endl;
        return "Ошибка: центральный маркер не найден";
    }

    // Декодирование 4 правых цифр
    for (int i = 0; i < 4; i++) {
        int digitStart = centerStart + 5 * width + i * 7 * width;
        int digit = decodeDigitWithWidth(profile, digitStart, RIGHT_PATTERNS, width);
        if (digit == -1) {
            cout << "  Ошибка декодирования правой цифры " << i << " для ширины " << width << "px" << endl;
            return "Ошибка декодирования правой цифры " + to_string(i);
        }
        result += to_string(digit);
    }

    cout << "  Успешно декодирован EAN-8: " << result << " для ширины " << width << "px" << endl;
    return result;
}